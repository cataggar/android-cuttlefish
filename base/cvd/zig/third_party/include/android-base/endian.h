#pragma once

#include <endian.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <cstdint>

#ifndef htonq
#define htonq(x) htobe64(x)
#endif

#ifndef ntohq
#define ntohq(x) be64toh(x)
#endif
