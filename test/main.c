#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

// volatile заставляет процессор всегда читать значение из RAM, 
// не кэшируя его в регистрах. Это важно для Memory Scanner-ов.
volatile int my_health = 100;

void on_damage(int amount) {
    if (my_health - amount <= 0) {
        printf("You died! Resurrecting...\n");
        my_health = 100;
    } else {
        printf("Took %d damage. Surviving.\n", amount);
        my_health -= amount;
    }
    fflush(stdout);
}

void on_health_pickup(int amount) {
    printf("Picked up %d health!\n", amount);
    my_health += amount;
    fflush(stdout);
}

int main() {
    srand(time(NULL)); // Теперь события будут реально случайными

    printf("--- IXERAM TRACING TEST ---\n");
    printf("PID: %d\n", getpid());
    printf("Health address: %p\n", (void*)&my_health);
    printf("---------------------------\n\n");
    fflush(stdout);
    
    while(1) {
        printf("Current Health: %d | ", my_health);
        
        int r = rand() % 3;
        
        if (r == 0) {
            on_damage(10);
        } else if (r == 1) {
            on_health_pickup(5);
        } else {
            printf("Evaded attack!\n");
            fflush(stdout);
        }
        
        sleep(2);
    }
    return 0;
}