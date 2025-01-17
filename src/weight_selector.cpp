#include <WS2812FX.h>

class WeightedRandomSelector {
private:
    struct Item {
        u_int8_t value;
        uint start;
        uint end;
    };

    Item* items;
    uint itemCount;
    uint totalWeight;

public:
    WeightedRandomSelector(int* weights, int itemCount) {
        this->itemCount = itemCount;
        items = new Item[itemCount];
        totalWeight = 0;

        // Calculate start and interval for each item
        for (int i = 0; i < itemCount; i++) {
            items[i].value = i;
            items[i].start = totalWeight;
            totalWeight += weights[i];
            items[i].end = totalWeight;          
        }
    }

    u_int8_t getRandomItem() {
        int randomNum = rand() % totalWeight;

        for (int i = 0; i < itemCount; i++) {
            if (randomNum >= items[i].start && randomNum < items[i].end) {
                return items[i].value;
            }
        }

        return 0;
    }
};