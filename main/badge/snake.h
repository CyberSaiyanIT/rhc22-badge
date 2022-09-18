#ifndef _SNAKE_H
#define _SNAKE_H

#include <stdio.h>
#include <stdlib.h>

#include "ui.h"

#define UI_SNAKE_MAX_BODY 64
#define UI_SNAKE_BODY_SIZE 20

#define SNAKE_MIN_SPEED 8
#define SNAKE_STEP_SPEED 3

enum SNAKE_DIR
{
    SNAKE_DIR_BEGIN = 10,
    SNAKE_DIR_UP = SNAKE_DIR_BEGIN,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_DOWN,
    SNAKE_DIR_RIGHT,
    SNAKE_DIR_END = SNAKE_DIR_RIGHT,
};

typedef struct 
{
    lv_obj_t *body[UI_SNAKE_MAX_BODY];
    lv_obj_t *eye; // the 'eye' on snake to locate it direction.
    lv_obj_t *food;
    enum SNAKE_DIR dir;
    int size;
    int speed;
} snake_t;

void snake_set_dir(int8_t dir);
void snake_reset(lv_obj_t *parent);
void snake_task(lv_task_t *arg);

#endif