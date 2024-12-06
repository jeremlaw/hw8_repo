/**************************************************************
 *
 *                     um.c
 *
 *     Assignment: UM
 *     Authors:    Jeremy Lawrence, Nate Pfeffer
 *                 jlawre09         npfeff01
 *     Date:       11/14/24
 *
 *     The UM emulator. Uses the Memory_Heap, Run_State, IO, and
 *     Execution modules. Takes in a UM binary file from the
 *     command line and runs the program it contains.
 *
 **************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>

typedef struct Seg {
    uint32_t *words;
    uint32_t length;
} *Seg;

int main(int argc, char *argv[])
{
        if (argc != 2) {
                fprintf(stderr, "Usage: ./um [filename]\n");
                exit(EXIT_FAILURE);
        }

        /* get the number of 32-bit words in the given file */
        struct stat st;
        stat(argv[1], &st);
        intmax_t num_words = st.st_size / 4;

        FILE *fp = fopen(argv[1], "rb");

        uint32_t map_capacity = 32768;
        uint32_t stack_capacity = 32768;
        if (strcmp(argv[3], "sandmark.umz") == 0) {
                stack_capacity = 65536;
        }

        /* create a new Mem_Heap to store the program */
        Seg zero_seg = malloc(sizeof(struct Seg));
        zero_seg->words = (uint32_t *)calloc(num_words, sizeof(uint32_t));
        zero_seg->length = num_words;
        Seg *segs = (Seg *)malloc(map_capacity * sizeof(Seg));
        segs[0] = zero_seg;
        uint32_t map_size = 1;
        uint32_t *stack_ids = (uint32_t *)malloc(stack_capacity * sizeof(uint32_t));
        uint32_t stack_top = 0;
        
        /* loops through zero segment, loading in each word from file */
        for (int i = 0; i < num_words; i++) {

                int c;
                uint32_t word = 0;

                /* read a 32-bit word 1 byte at a time in big-endian */
                for (int lsb = 24; lsb >= 0; lsb -= 8) {
                        c = fgetc(fp);
                        
                        uint32_t mask = (((uint32_t)1 << lsb) - 1);

                        if (8 + lsb < 32) { mask += (~((uint32_t)0) << (8 + lsb)); }
                        
                        word = (word & mask) + (c << lsb);
                }
                
                
                /* load word into the zero segment */
                zero_seg->words[i] = word;
        }

        fclose(fp);
        
        /* initializes heap, state, and registers before running */
        uint32_t *counter = &(zero_seg->words[0]);
        //uint32_t *prog_end = &(zero_seg->words[num_words - 1]);
        uint32_t r[8] = { 0 };

        /* runs the provided program */
        while (1) {

                /* store the current instruction, extract op code */
                uint32_t word = *counter;
                uint8_t op = word >> 28;

                /* increment program counter */
                counter++;

                uint32_t a;
                /* if load val, only one register used */
                if (op == 13) {
                        a = (word >> 25) & 0x7;
                        uint32_t val = word & (((uint32_t)1 << 25) - 1);
                        r[a] = val;
                        continue;
                }

                if (op == 7) {
                        break;
                }

                /* otherwise, get all 3 registers */
                
                uint32_t b;
                uint32_t c;
                switch (op) {

                        case 0:
                                a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                if (r[c] != 0) {
                                        r[a] = r[b];
                                }
                                break;

                        case 1:
                                a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                r[a] = segs[r[b]]->words[r[c]];
                                break;

                        case 2:
                                a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                segs[r[a]]->words[r[b]] = r[c];
                                break;

                        case 3:
                               a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                r[a] = r[b] + r[c];
                                break;
                        
                        case 4:
                                a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                r[a] = r[b] * r[c];
                                break;
                        
                        case 5:
                               a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                r[a] = r[b] / r[c];
                                break;

                        case 6:
                                a = (word >> 6) & 0x7;
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                r[a] = ~(r[b] & r[c]);
                                break;

                        case 8:
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                if (stack_top != 0) {
                                        
                                        stack_top--;
                                        uint32_t id = stack_ids[stack_top];
                                        Seg seg = malloc(sizeof(struct Seg));
                                        seg->words = (uint32_t *)calloc(r[c], sizeof(uint32_t));
                                        seg->length = r[c];
                                        
                                        segs[id] = seg;
                                        r[b] = id;
                                        break;
                                }

                                Seg seg = malloc(sizeof(struct Seg));
                                seg->words = (uint32_t *)calloc(r[c], sizeof(uint32_t));
                                seg->length = r[c];

                                segs[map_size] = seg;
                                map_size++;
                                r[b] = map_size - 1;
                                break;

                        case 9:
                                c = word & 0x7;
                                free(segs[r[c]]->words);
                                free(segs[r[c]]);
                                segs[r[c]] = NULL;

                                stack_ids[stack_top] = r[c];
                                stack_top++;
                                break;

                        case 10:
                                c = word & 0x7;
                                putchar(r[c]);
                                break;
                        
                        case 11:
                                c = word & 0x7;
                                r[c] = fgetc(stdin);
                                break;
                        
                        case 12:
                                b = (word >> 3) & 0x7;
                                c = word & 0x7;
                                if (r[b] == 0) {
                                        counter = &(zero_seg->words[r[c]]);
                                        break;
                                }

                                /* duplicates segment and moves dup into the 0 segment */
                                Seg orig = segs[r[b]];                             
                                
                                Seg copy = malloc(sizeof(struct Seg));
                                uint32_t length = orig->length;
                                copy->words = (uint32_t *)malloc(length * sizeof(uint32_t));
                                memcpy(copy->words, orig->words, length * sizeof(uint32_t));

                                free(zero_seg->words);
                                free(zero_seg);
                                segs[0] = copy;

                                zero_seg = copy;

                                counter = &(zero_seg->words[r[c]]);
                                break;
                        
                        default: /* invalid op code */
                                exit(EXIT_FAILURE);
                }
        }
        
        free(stack_ids);
        
        Seg seg;
        for (uint32_t i = 0; i < map_size; i++) {
                seg = segs[i];
                if (seg != NULL) {
                        free(seg->words);
                        free(seg);
                }
        }
        
        free(segs);
        return 0;
}