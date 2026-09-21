# Linux x86 fault provenance window / Linux x86 fault provenance window

null read의 직접 원인은 확인됐지만, 24-byte window에는 EAX를 마지막으로 쓴 명령이 없습니다. Linux x86 in-process fault observation의 live instruction window를 EIP 전 64 bytes와 후 16 bytes, 합계 80 bytes로 넓힙니다. fault byte의 offset을 함께 출력해 runtime code를 외부 disassembler로 bounded 분석할 수 있게 합니다.

*The direct null-read cause is confirmed, but the 24-byte window does not contain the instruction that last wrote EAX. Expand the Linux x86 in-process fault observation to 64 live instruction bytes before EIP and 16 after it, 80 bytes total. Print the fault-byte offset so bounded runtime code can be analyzed with an external disassembler.*

이 작업은 실행을 재개하거나 HLE 결과를 바꾸지 않습니다. 기존 fault context처럼 원본 이미지를 저장하지 않고, fault 주변의 bounded live bytes만 console과 작업 증거에 사용합니다. window가 EAX producer를 포함하지 않으면 producer는 계속 미확정으로 남깁니다.

*This task neither resumes execution nor changes HLE results. Like the existing fault context, it does not save original images; it uses only bounded live bytes around the fault in console output and work evidence. If the window does not contain the EAX producer, that producer remains unresolved.*