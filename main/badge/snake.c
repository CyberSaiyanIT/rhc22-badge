#include "snake.h"

const char ui_snake_eyes[][4] = {"' '", ": ", ". .", " :"};

static snake_t snake;

void snake_set_dir(int8_t dir)
{
    dir += snake.dir;
    if (dir < SNAKE_DIR_BEGIN)
        dir = SNAKE_DIR_END;
    if (dir > SNAKE_DIR_END)
        dir = SNAKE_DIR_BEGIN;
    snake.dir = dir;
    lv_label_set_text(snake.eye, ui_snake_eyes[snake.dir - SNAKE_DIR_BEGIN]);
}

static void snake_add_body(lv_obj_t *parent)
{
    if (snake.size >= UI_SNAKE_MAX_BODY)
        return;

    int index = snake.size++;
    snake.body[index] = lv_btn_create(parent, NULL);
    lv_btn_toggle(snake.body[index]);
    lv_obj_set_size(snake.body[index], UI_SNAKE_BODY_SIZE, UI_SNAKE_BODY_SIZE);
}

void snake_reset(lv_obj_t *parent)
{
    if(snake_task_handle)
        lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_OFF);

    srand(lv_tick_get());

    for (int i = 0; i < snake.size; i++)
    {
        lv_obj_del(snake.body[i]);
        snake.body[i] = NULL;
    }
    snake.size = 0;
    if (snake.food)
    {
        lv_obj_del(snake.food);
        snake.food = NULL;
    }

    snake_add_body(parent); // head
    snake.eye = lv_label_create(snake.body[0], NULL);
    snake_set_dir(SNAKE_DIR_RIGHT);
    lv_obj_set_pos(snake.body[0], UI_SNAKE_BODY_SIZE * 3, UI_SNAKE_BODY_SIZE * 6);

    snake_add_body(parent);
    lv_obj_set_pos(snake.body[1], UI_SNAKE_BODY_SIZE * 2, UI_SNAKE_BODY_SIZE * 6);
    snake_add_body(parent);
    lv_obj_set_pos(snake.body[2], UI_SNAKE_BODY_SIZE * 1, UI_SNAKE_BODY_SIZE * 6);

    snake.speed = SNAKE_MIN_SPEED;
}

static int counter = 0;

void snake_task(lv_task_t *arg)
{
    // not current page, ignore.
    if (lv_scr_act() != screen_snake){
        lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_OFF);
        return;
    }

    if(counter < snake.speed) {
        counter++;
        return;
    }
    
    counter = 0;

    lv_obj_t *parent = screen_snake;
    lv_coord_t x = lv_obj_get_x(snake.body[0]);
    lv_coord_t y = lv_obj_get_y(snake.body[0]);

    switch (snake.dir)
    {
        case SNAKE_DIR_UP:
            y -= UI_SNAKE_BODY_SIZE;
            break;
        case SNAKE_DIR_DOWN:
            y += UI_SNAKE_BODY_SIZE;
            break;
        case SNAKE_DIR_LEFT:
            x -= UI_SNAKE_BODY_SIZE;
            break;
        case SNAKE_DIR_RIGHT:
            x += UI_SNAKE_BODY_SIZE;
            break;
    }

    // check if head hit any wall.
    if (x >= LV_HOR_RES || x < 0 || y >= LV_VER_RES || y < 0)
    {
        snake_reset(screen_snake);
        return;
    }
    // check if head hit any body.
    for (int i = 1; i < snake.size; i++)
    {
        if (x == lv_obj_get_x(snake.body[i]) && y == lv_obj_get_y(snake.body[i]))
        {
            snake_reset(screen_snake);
            return;
        }
    }
    // check if head hit any food.
    if (snake.food)
    {
        if (x == lv_obj_get_x(snake.food) && y == lv_obj_get_y(snake.food))
        {
            lv_obj_del(snake.food);
            snake.food = NULL;
            snake_add_body(parent);

            if(snake.size % SNAKE_STEP_SPEED == 0 && snake.speed > 0){
                snake.speed--;
                ESP_LOGI(__FILE__, "Snake speed is: %d", snake.speed);
            }
        }
    }
    // if no food exists, we need to generate one in random place.
    else
    {
        snake.food = lv_btn_create(screen_snake, NULL);
        lv_obj_set_size(snake.food, UI_SNAKE_BODY_SIZE, UI_SNAKE_BODY_SIZE);
        lv_obj_set_pos(snake.food,
                       (rand() % LV_HOR_RES) / UI_SNAKE_BODY_SIZE * UI_SNAKE_BODY_SIZE,
                       (rand() % LV_VER_RES) / UI_SNAKE_BODY_SIZE * UI_SNAKE_BODY_SIZE);
    }

    // now we can move the body.
    for (int i = snake.size - 1; i >= 1; i--)
    {
        lv_coord_t cx = lv_obj_get_x(snake.body[i - 1]);
        lv_coord_t cy = lv_obj_get_y(snake.body[i - 1]);
        lv_obj_set_pos(snake.body[i], cx, cy);
    }
    lv_obj_set_pos(snake.body[0], x, y); // head is last one.
}