#pragma once

typedef struct _tagOpenH264Version {
  unsigned int uMajor;
  unsigned int uMinor;
  unsigned int uRevision;
  unsigned int uReserved;
} OpenH264Version;

#define OPENH264_MAJOR 2
#define OPENH264_MINOR 4
#define OPENH264_REVISION 1
#define OPENH264_RESERVED 0

static const OpenH264Version g_stCodecVersion = {
    OPENH264_MAJOR, OPENH264_MINOR, OPENH264_REVISION, OPENH264_RESERVED};
