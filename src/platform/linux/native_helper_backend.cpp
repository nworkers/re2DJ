#include "re2dj/platform/linux/native_helper_backend.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "../native_helper_protocol.h"

namespace re2dj::platform::linux
{
namespace
{
namespace protocol = native_protocol;
constexpr std::uint32_t kTransferLimit = 4096;
void Error(std::string* error, const char* message) { if (error != nullptr) *error = message; }
bool ReadExact(int fd, void* value, std::uint32_t size) { auto* p=static_cast<std::uint8_t*>(value); for(std::uint32_t n=0;n<size;) { const ssize_t r=read(fd,p+n,size-n); if(r<0 && errno==EINTR) continue; if(r<=0) return false; n+=static_cast<std::uint32_t>(r); } return true; }
bool WriteExact(int fd, const void* value, std::uint32_t size) { const auto* p=static_cast<const std::uint8_t*>(value); for(std::uint32_t n=0;n<size;) { const ssize_t r=write(fd,p+n,size-n); if(r<0 && errno==EINTR) continue; if(r<=0) return false; n+=static_cast<std::uint32_t>(r); } return true; }
std::uint64_t EventId(std::uint32_t low,std::uint32_t high) { return static_cast<std::uint64_t>(low)|(static_cast<std::uint64_t>(high)<<32); }
}

class NativeHelperBackend::Impl
{
public:
 explicit Impl(std::filesystem::path path):path_(std::move(path)) {}
 ~Impl(){ Stop(); }
 bool Prepare(std::span<const std::uint8_t> file,const exe::PeImageInfo& info,runtime::GuestAddress requested,runtime::LoadedPeImage* loaded,std::string* error) {
  if(state_!=State::Idle||loaded==nullptr||file.empty()||file.size()>protocol::kMaximumPayloadSize-sizeof(protocol::LoadImageRequest)||!exe::IsGuestExecutable(info)){Error(error,"invalid Linux native helper image arguments");return false;}
  const std::uint32_t base=requested.value()==0?static_cast<std::uint32_t>(info.image_base):requested.value(); if(!Launch(error))return false;
  protocol::LoadImageRequest req{base,static_cast<std::uint32_t>(file.size())}; std::vector<std::uint8_t> payload(sizeof(req)+file.size()); std::memcpy(payload.data(),&req,sizeof(req)); std::memcpy(payload.data()+sizeof(req),file.data(),file.size());
  if(!Send(protocol::MessageType::kLoadImage,payload.data(),static_cast<std::uint32_t>(payload.size()),error)){Stop();return false;} protocol::LoadResult result; if(!Receive(protocol::MessageType::kLoadResult,&result,error)||result.success!=1||result.load_base!=base||result.entry_point!=base+info.entry_point_rva||result.import_count>protocol::kMaximumImportCount){if(error==nullptr||error->empty())Error(error,"Linux helper returned inconsistent image metadata");Stop();return false;}
  loaded->load_base=runtime::GuestAddress(result.load_base); loaded->entry_point=runtime::GuestAddress(result.entry_point); loaded->imports.clear();
  for(std::uint32_t i=0;i<result.import_count;++i){ runtime::ImportGate gate; if(!Metadata(&gate,error)){Stop();return false;} loaded->imports.push_back(std::move(gate)); }
  const auto* tls=info.Directory(exe::PeDirectoryIndex::kTls); loaded->tls_directory_rva=tls==nullptr?0:tls->virtual_address; loaded->tls_directory_size=tls==nullptr?0:tls->size; state_=State::Prepared; return true;
 }
 bool Start(std::string* error){ if(state_!=State::Prepared||!Send(protocol::MessageType::kStart,nullptr,0,error)){Error(error,"Linux helper cannot start image");Stop();return false;}state_=State::Running;return true; }
 bool Wait(runtime::ExecutionEvent* event,std::string* error){ if(state_!=State::Running||event==nullptr){Error(error,"Linux helper is not running");return false;} protocol::ExecutionEvent packet; if(!Receive(protocol::MessageType::kExecutionEvent,&packet,error)){Stop();return false;} event->event_id=EventId(packet.event_id_low,packet.event_id_high);event->thread_id=packet.thread_id;event->instruction_pointer=runtime::GuestAddress(packet.instruction_pointer);event->stack_pointer=runtime::GuestAddress(packet.stack_pointer);event->gate_address=runtime::GuestAddress(packet.gate_address);event->status_code=packet.status_code; if(packet.kind==static_cast<std::uint32_t>(protocol::EventKind::kImportGate)){event->kind=runtime::ExecutionEventKind::kImportGate;pending_=event->event_id;state_=State::Pending;return true;} if(packet.kind==static_cast<std::uint32_t>(protocol::EventKind::kProcessExit)){event->kind=runtime::ExecutionEventKind::kProcessExit; state_=State::Exited; Close(); return true;} event->kind=runtime::ExecutionEventKind::kFault;state_=State::Failed;return true; }
 bool Memory(runtime::GuestAddress address,std::span<std::uint8_t> bytes,std::string* error){ if(state_!=State::Pending||bytes.size()>kTransferLimit){Error(error,"guest memory is unavailable");return false;}protocol::ReadMemoryRequest req{address.value(),static_cast<std::uint32_t>(bytes.size())};if(!Send(protocol::MessageType::kReadMemory,&req,sizeof(req),error))return false;protocol::MessageHeader h;if(!Header(&h,error)||h.type!=static_cast<std::uint32_t>(protocol::MessageType::kMemoryData)||h.payload_size!=bytes.size()||(!bytes.empty()&&!ReadExact(out_,bytes.data(),static_cast<std::uint32_t>(bytes.size())))){Error(error,"cannot read Linux helper memory");return false;}return true; }
 bool Write(runtime::GuestAddress address,std::span<const std::uint8_t> bytes,std::string* error){if(state_!=State::Pending||bytes.size()>kTransferLimit){Error(error,"guest memory is unavailable");return false;}protocol::ReadMemoryRequest req{address.value(),static_cast<std::uint32_t>(bytes.size())};std::vector<std::uint8_t> p(sizeof(req)+bytes.size());std::memcpy(p.data(),&req,sizeof(req));if(!bytes.empty())std::memcpy(p.data()+sizeof(req),bytes.data(),bytes.size());protocol::WriteMemoryResult result;if(!Send(protocol::MessageType::kWriteMemory,p.data(),static_cast<std::uint32_t>(p.size()),error)||!Receive(protocol::MessageType::kWriteResult,&result,error)||result.success!=1||result.size!=bytes.size()){Error(error,"cannot write Linux helper memory");return false;}return true;}
 bool Complete(const runtime::ImportCompletion& completion,std::string* error){if(state_!=State::Pending||completion.event_id!=pending_){Error(error,"invalid Linux helper import completion");return false;}protocol::CompleteImport p; p.event_id_low=static_cast<std::uint32_t>(completion.event_id);p.event_id_high=static_cast<std::uint32_t>(completion.event_id>>32);p.eax=completion.eax;p.edx=completion.edx;p.stack_bytes_to_pop=completion.stack_bytes_to_pop;p.action=completion.action==runtime::ImportCompletionAction::kContinue?0:1;if(!Send(protocol::MessageType::kCompleteImport,&p,sizeof(p),error)){Stop();return false;}state_=State::Running;return true;}
 void Stop(){Close();if(pid_>0){kill(pid_,SIGKILL);while(waitpid(pid_,nullptr,0)<0&&errno==EINTR){}pid_=-1;}if(state_!=State::Exited)state_=State::Failed;}
private:
 enum class State{Idle,Prepared,Running,Pending,Exited,Failed};
 bool Launch(std::string* error){int a[2]={-1,-1},b[2]={-1,-1};if(pipe(a)!=0||pipe(b)!=0){Error(error,"cannot create Linux helper pipes");return false;}pid_=fork();if(pid_==0){dup2(a[0],0);dup2(b[1],1);close(a[0]);close(a[1]);close(b[0]);close(b[1]);execl(path_.c_str(),path_.c_str(),static_cast<char*>(nullptr));_exit(127);}if(pid_<0){Error(error,"cannot fork Linux helper");return false;}close(a[0]);close(b[1]);in_=a[1];out_=b[0];return true;}
 void Close(){if(in_>=0){close(in_);in_=-1;}if(out_>=0){close(out_);out_=-1;}}
 bool Send(protocol::MessageType type,const void* p,std::uint32_t n,std::string* e){protocol::MessageHeader h;h.type=static_cast<std::uint32_t>(type);h.payload_size=n;if(!WriteExact(in_,&h,sizeof(h))||(n&&!WriteExact(in_,p,n))){Error(e,"cannot write Linux helper packet");return false;}return true;}
 bool Header(protocol::MessageHeader* h,std::string* e){if(!ReadExact(out_,h,sizeof(*h))||h->magic!=protocol::kMagic||h->version!=protocol::kVersion||h->payload_size>protocol::kMaximumPayloadSize){Error(e,"invalid Linux helper packet");return false;}return true;}
 template<class T> bool Receive(protocol::MessageType expected,T* p,std::string* e){protocol::MessageHeader h;if(!Header(&h,e))return false;if(h.type==static_cast<std::uint32_t>(protocol::MessageType::kError)){std::vector<char> message(h.payload_size+1,0);if(h.payload_size!=0&&!ReadExact(out_,message.data(),h.payload_size)){Error(e,"cannot read Linux helper error");return false;}if(e!=nullptr)*e=message.data();return false;}if(h.type!=static_cast<std::uint32_t>(expected)||h.payload_size!=sizeof(T)||!ReadExact(out_,p,sizeof(T))){Error(e,"unexpected Linux helper packet");return false;}return true;}
 bool Metadata(runtime::ImportGate* gate,std::string* e){protocol::MessageHeader h;if(!Header(&h,e)||h.type!=static_cast<std::uint32_t>(protocol::MessageType::kImportMetadata)||h.payload_size<sizeof(protocol::ImportMetadata)){Error(e,"invalid import metadata");return false;}protocol::ImportMetadata m;if(!ReadExact(out_,&m,sizeof(m))||m.module_size==0||m.module_size>protocol::kMaximumImportStringSize||m.name_size>protocol::kMaximumImportStringSize||h.payload_size!=sizeof(m)+m.module_size+m.name_size){Error(e,"invalid import metadata payload");return false;}gate->module.resize(m.module_size);gate->name.resize(m.name_size);if(!ReadExact(out_,gate->module.data(),m.module_size)||(m.name_size&&!ReadExact(out_,gate->name.data(),m.name_size))){Error(e,"cannot read import metadata");return false;}gate->address=runtime::GuestAddress(m.gate_address);gate->by_ordinal=m.by_ordinal!=0;gate->ordinal=static_cast<std::uint16_t>(m.ordinal);return true;}
 std::filesystem::path path_;pid_t pid_=-1;int in_=-1,out_=-1;State state_=State::Idle;std::uint64_t pending_=0;
};
NativeHelperBackend::NativeHelperBackend(std::filesystem::path path):impl_(std::make_unique<Impl>(std::move(path))){}
NativeHelperBackend::~NativeHelperBackend()=default;
bool NativeHelperBackend::PrepareImage(std::span<const std::uint8_t>a,const exe::PeImageInfo&b,runtime::GuestAddress c,runtime::LoadedPeImage*d,std::string*e){return impl_->Prepare(a,b,c,d,e);} bool NativeHelperBackend::Start(std::string*e){return impl_->Start(e);} bool NativeHelperBackend::WaitForEvent(runtime::ExecutionEvent*a,std::string*e){return impl_->Wait(a,e);} bool NativeHelperBackend::ReadMemory(runtime::GuestAddress a,std::span<std::uint8_t>b,std::string*e){return impl_->Memory(a,b,e);} bool NativeHelperBackend::WriteMemory(runtime::GuestAddress a,std::span<const std::uint8_t>b,std::string*e){return impl_->Write(a,b,e);} bool NativeHelperBackend::CompleteImport(const runtime::ImportCompletion&a,std::string*e){return impl_->Complete(a,e);} void NativeHelperBackend::RequestStop(){impl_->Stop();}
}  // namespace re2dj::platform::linux
