#include <stdio.h>

#define ROWS 7
#define COLS 12

#define BIN_SIZE 5
#define MAX_VALUE 4095
#define NUM_BINS (MAX_VALUE / BIN_SIZE + 1)

short dados[ROWS][COLS] = {
    {20, 20, 20, 300, 400, 20, 300, 20, 20, 20, 20, 20},
    {20, 20, 20, 20, 300, 300, 20, 20, 400, 20, 20, 20},
    {20, 20, 20, 20, 20, 300, 300, 400, 20, 20, 0, 0},
    {0, 0, 20, 20, 20, 20, 300, 300, 400, 20, 20, 20},
    {20, 20, 20, 400, 20, 20, 20, 300, 300, 20, 20, 20},
    {20, 20, 400, 20, 20, 20, 20, 300, 300, 20, 20, 20},
    {20, 20, 20, 20, 20, 20, 300, 300, 20, 20, 20, 20}
};

int main()
{
    int hist[NUM_BINS] = {0};

    // 1. Preencher histograma
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {

            int v = dados[i][j];

            if (v <= 0) continue; // ignora ruído

            int bin = v / BIN_SIZE;

            if (bin >= NUM_BINS) continue;

            hist[bin]++;
        }
    }

    // 2. Encontrar bin mais frequente
    int maxCount = 0;
    int bestBin = 0;

    for (int i = 0; i < NUM_BINS; i++) {
        if (hist[i] > maxCount) {
            maxCount = hist[i];
            bestBin = i;
        }
    }

    // 3. Converter bin para valor representativo
    int standby = bestBin * BIN_SIZE;

    printf("Standby estimado: %d\n", standby);
    printf("Faixa: [%d - %d]\n",
           bestBin * BIN_SIZE,
           bestBin * BIN_SIZE + BIN_SIZE - 1);

    return 0;
}