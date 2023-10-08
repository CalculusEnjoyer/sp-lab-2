#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <string.h>
#include "hashmap.h"

const int MAX_WORD_SIZE = 256;

struct automate {
    int start_state;
    int final_state;
    wchar_t degeneration_char;
};

struct word {
    wchar_t *chars;
    int size;
};

struct states {
    int *states;
    int size;
};

struct auto_config {
    int state_size;
    int start_state;
    int final_states_size;
    int automate_size;
    int *final_states;
    struct automate *automates;
};

uint64_t automate_hash(const void *automate, uint64_t seed0, uint64_t seed1) {
    const struct automate *current_word = automate;
    return hashmap_sip(&current_word->degeneration_char, 1, seed0, seed1);;
}


int automate_compare(const void *a, const void *b, void *udata) {
    const struct automate *ua = a;
    const struct automate *ub = b;
    if (ua->degeneration_char == ub->degeneration_char && ua->start_state == ub->start_state) {
        return 0;
    } else {
        return -1;
    }
}

struct word concatenateWords(struct word word1, struct word word2, struct word word3) {
    // Calculate the size of the concatenated string
    int totalSize = word1.size + word2.size + word3.size;

    // Allocate memory for the concatenated string
    wchar_t *concatenatedChars = (wchar_t *) malloc((totalSize + 1) * sizeof(wchar_t));

    // Check for memory allocation errors
    if (concatenatedChars == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Copy the characters from each word to the concatenated string
    wcscpy(concatenatedChars, word1.chars);
    wcscat(concatenatedChars, word2.chars);
    wcscat(concatenatedChars, word3.chars);

    // Create a new struct word to hold the concatenated result
    struct word result;
    result.chars = concatenatedChars;
    result.size = totalSize;

    return result;
}

void parse_config(const char *filename, struct auto_config *config) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Read state_size, start_state, final_states_size, and automate_size
    fscanf(file, "%d", &config->state_size);
    fscanf(file, "%d", &config->start_state);

    // Allocate memory for final_states and automates arrays
    config->final_states = (int *) malloc(sizeof(int) * MAX_WORD_SIZE);
    config->automates = (struct automate *) malloc(sizeof(struct automate) * MAX_WORD_SIZE);

    // Read final_states
    int state;
    int i = 0;
    while (fscanf(file, "%d", &state) == 1) {
        config->final_states[i] = state;
        i++;
    }
    config->final_states_size = i;

    // Read and ignore the blank line
    char line[MAX_WORD_SIZE];
    fgets(line, sizeof(line), file);

    // Read and parse automates
    i = 0;
    struct automate automate;
    while (fscanf(file, "%d %lc %d", &automate.start_state, &automate.degeneration_char, &automate.final_state) == 3) {

        printf("Start: %d, Degeneration Char: %lc, Final: %d\n", automate.start_state, automate.degeneration_char,
               automate.final_state);
        config->automates[i] = automate;
        i++;
    }
    config->automate_size = i;

    fclose(file);
}

// Helper function to check if a state is in the list of states
bool isStateInList(int state, const struct states *stateList) {
    for (int i = 0; i < stateList->size; i++) {
        if (state == stateList->states[i]) {
            return true;
        }
    }
    return false;
}

// Helper function to add a state to the list of states
void addStateToList(int state, struct states *stateList) {
    if (!isStateInList(state, stateList)) {
        stateList->states[stateList->size++] = state;
    }
}

struct states find_possible_states(struct states init_states, struct states *reached_states, struct auto_config config) {
    struct states transisiobal_states;
    transisiobal_states.states = (int *) malloc(config.state_size * sizeof(int));
    transisiobal_states.size = 0;
    bool is_over = true;
    for (int i = 0; i < init_states.size; i++) {
        int current_state = init_states.states[i];
        for (int j = 0; j < config.automate_size; j++) {
            if (!isStateInList(current_state, reached_states)) {
                addStateToList(current_state, reached_states);
            }
            if (config.automates[j].start_state == current_state &&
                config.automates[j].start_state != config.automates[j].final_state &&
                !isStateInList(config.automates[j].final_state, reached_states)) {
                addStateToList(config.automates[j].final_state, &transisiobal_states);
                is_over = false;
            }
        }
    }
    if (!is_over) {
        struct states next_states = find_possible_states(transisiobal_states, reached_states, config);
        for (int k = 0; k < next_states.size; k++) {
            addStateToList(next_states.states[k], reached_states);
        }
    }

    for (int i = 0; i < init_states.size; i++) {
        addStateToList(init_states.states[i], reached_states);
    }
    return transisiobal_states;
}

int find_final_state(struct word input_word, struct auto_config config, int init_state) {
    int current_state = init_state;
    struct hashmap *map = hashmap_new(sizeof(struct automate), 0, 0, 0, automate_hash, automate_compare, NULL, NULL);

    for (int i = 0; i < config.automate_size; i++) {
        hashmap_set(map, &config.automates[i]);
    }

    for (int i = 0; i < input_word.size; i++) {
        struct automate next_auto;
        next_auto.degeneration_char = input_word.chars[i];
        next_auto.start_state = current_state;
        struct automate *curr_auto = hashmap_get(map, &next_auto);
        if (curr_auto == NULL) {
            return -1;
        }
        current_state = curr_auto->final_state;
    }
    return current_state;
}

int check_if_final_state_proper(int final_state, struct auto_config config) {
    for (int i = 0; i < config.final_states_size; i++) {
        if (config.final_states[i] == final_state) {
            return 1;
        }
    }
    return 0;
}

void printStates(struct states s) {
    if (s.states == NULL || s.size <= 0) {
        printf("No states to print\n");
        return;
    }

    printf("States: ");
    for (int i = 0; i < s.size; i++) {
        printf("%d ", s.states[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "uk_UA.UTF-8");
    size_t requiredSize = mbstowcs(NULL, argv[1], 10);
    wchar_t *wcharStr = (wchar_t *)malloc((requiredSize + 1) * sizeof(wchar_t));
    mbstowcs(wcharStr, argv[1], requiredSize + 1);

    struct word w0_word;
    w0_word.chars = wcharStr;
    w0_word.size = wcslen(wcharStr);

    struct auto_config config;
    parse_config("auto.txt", &config);

    struct states init_states;
    init_states.states = (int *) malloc(config.state_size * sizeof(int));
    init_states.size = 0;
    addStateToList(config.start_state, &init_states);

    struct states reached_states;
    reached_states.states = (int *) malloc(config.state_size * sizeof(int));
    reached_states.size = 0;

    struct states states_after_w0;
    states_after_w0.states = (int *) malloc(config.state_size * sizeof(int));
    states_after_w0.size = 0;

    struct states states_after_w2;
    states_after_w2.states = (int *) malloc(config.state_size * sizeof(int));
    states_after_w2.size = 0;


    find_possible_states(init_states, &reached_states, config);
    printStates(reached_states);
    for (int i = 0; i < reached_states.size; i ++) {
        int state_after_w0 = find_final_state(w0_word, config, reached_states.states[i]);
        if (state_after_w0 == -1) {
            continue;
        }
        addStateToList(state_after_w0, &states_after_w0);
    }
    printStates(states_after_w0);
    find_possible_states(states_after_w0, &states_after_w2, config);
    printStates(states_after_w2);
    for (int i = 0; i < states_after_w2.size; ++i) {
        if (check_if_final_state_proper(states_after_w2.states[i], config)){
            printf("good");
            return 0;
        }
    }
    printf("no. it is not good");

    return 0;
}
