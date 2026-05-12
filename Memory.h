
#ifndef MEMORY_H
#define MEMORY_H

#define ROWS 7
#define COLS 48

void memoryBegin();
void saveReading(int r, int c, int valor);
int readReading(int r, int c);
void debugPrintNVSMemory();
void clearNVSMemory();
void handleSerialDebugCommands();

#endif
