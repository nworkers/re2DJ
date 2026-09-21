# 작업 324: Linux 첫 import 인자 텍스트 관찰 / Task 324: Linux first-import argument text observation

## 목표 / Goal

첫 import의 nonzero arg0에서 최대 4096바이트의 NUL 종료 텍스트를 읽어 실제 호출 인자 관찰을 확장한다.

*Extend actual first-import argument observation by reading bounded NUL-terminated text of at most 4096 bytes from nonzero arg0.*

## 작업 / Work

1. runner 결과에 first-argument text 관찰 여부와 텍스트를 추가한다.
2. 8바이트 stack read 뒤 제한된 read 한 번으로 첫 NUL을 찾는다.
3. CHD와 directory HDD 출력에 관찰 text를 표시한다.
4. Linux x64/x86 actual CHD run 및 CTest를 수행하고 결과를 기록한다.

*1. Add first-argument text observation presence and text to runner results.
2. After the eight-byte stack read, find the first NUL with one bounded read.
3. Display observed text in CHD and directory-HDD output.
4. Run actual Linux x64/x86 CHD execution and CTest, then record results.*

## 완료 기준 / Completion criteria

read는 4096바이트를 넘지 않고, NUL이 없거나 read에 실패하면 실행 오류가 된다. 양쪽 실제 run은 같은 text를 보고하며, API binding 또는 original-code continuation을 수행하지 않는다.

*The read never exceeds 4096 bytes; missing NUL or read failure is an execution error. Both actual runs report the same text and do not perform API binding or original-code continuation.*
