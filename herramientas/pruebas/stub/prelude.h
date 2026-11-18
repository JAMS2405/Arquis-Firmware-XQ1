#pragma once
#include <stdint.h>
#include <stdio.h>
#define TEST(n,b) (!!((n)&(1UL<<(b))))
#define SBI(n,b) ((n)|=(1UL<<(b)))
#define CBI(n,b) ((n)&=~(1UL<<(b)))
#define WITHIN(N,L,H) ((N)>=(L)&&(N)<=(H))
#define ENABLED(x) 1
#define TERN_(a,b)
#define ALL(...) 0
#define SERIAL_ECHO_MSG(...)
#define SERIAL_ECHO_START()
#define SERIAL_ECHOPGM(x)
#define SERIAL_ECHO(x)
#define SERIAL_CHAR(c)
#define SERIAL_EOL()
