#ifndef HEADER_H
#define HEADER_H

static const long long NANOCONVERTER = 1000000000;

typedef struct sharedStruct {
  long long ossTimer;
  int sigNotReceived;
} sharedStruct;

struct msgformat {
  long mType;
  char mText[80];
};

#endif
