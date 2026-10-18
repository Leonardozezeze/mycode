#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// 游戏区域大小
#define WIDTH 60
#define HEIGHT 20

// 方向定义
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

// 蛇身节点
typedef struct SnakeNode {
    int x, y;
    struct SnakeNode *next;
} SnakeNode;

// 全局变量
SnakeNode *head = NULL;  // 蛇头
int direction = RIGHT;   // 初始方向
int food_x, food_y;      // 食物位置
int score = 0;           // 分数
int game_over = 0;       // 游戏结束标志

// 初始化蛇
void init_snake() {
    head = (SnakeNode *)malloc(sizeof(SnakeNode));
    head->x = WIDTH / 2;
    head->y = HEIGHT / 2;
    head->next = NULL;

    // 添加初始蛇身（3节）
    for (int i = 1; i < 3; i++) {
        SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
        node->x = head->x - i;
        node->y = head->y;
        node->next = head;
        head = node;
    }
}

// 生成食物
void generate_food() {
    srand(time(NULL));
    int valid_position;

    do {
        valid_position = 1;
        food_x = rand() % (WIDTH - 2) + 1;
        food_y = rand() % (HEIGHT - 2) + 1;

        // 确保食物不在蛇身上
        SnakeNode *current = head;
        while (current != NULL) {
            if (current->x == food_x && current->y == food_y) {
                valid_position = 0;
                break;
            }
            current = current->next;
        }
    } while (!valid_position);
}

// 绘制游戏界面
void draw() {
    clear();

    // 绘制边框
    for (int i = 0; i < WIDTH; i++) {
        mvaddch(0, i, '=');
        mvaddch(HEIGHT - 1, i, '=');
    }
    for (int i = 0; i < HEIGHT; i++) {
        mvaddch(i, 0, '|');
        mvaddch(i, WIDTH - 1, '|');
    }

    // 绘制蛇
    SnakeNode *current = head;
    while (current != NULL) {
        mvaddch(current->y, current->x, 'O');
        current = current->next;
    }

    // 绘制食物
    mvaddch(food_y, food_x, '*');

    // 显示分数
    mvprintw(HEIGHT + 1, 0, "分数: %d", score);
    mvprintw(HEIGHT + 2, 0, "使用方向键控制，按 q 退出");

    refresh();
}

// 移动蛇
void move_snake() {
    // 创建新蛇头
    SnakeNode *new_head = (SnakeNode *)malloc(sizeof(SnakeNode));
    new_head->x = head->x;
    new_head->y = head->y;

    // 根据方向移动
    switch (direction) {
        case UP:
            new_head->y--;
            break;
        case DOWN:
            new_head->y++;
            break;
        case LEFT:
            new_head->x--;
            break;
        case RIGHT:
            new_head->x++;
            break;
    }

    // 检测碰撞（墙壁）
    if (new_head->x <= 0 || new_head->x >= WIDTH - 1 ||
        new_head->y <= 0 || new_head->y >= HEIGHT - 1) {
        game_over = 1;
        free(new_head);
        return;
    }

    // 检测碰撞（自身）
    SnakeNode *current = head;
    while (current != NULL) {
        if (current->x == new_head->x && current->y == new_head->y) {
            game_over = 1;
            free(new_head);
            return;
        }
        current = current->next;
    }

    // 添加到蛇头
    new_head->next = head;
    head = new_head;

    // 检测是否吃到食物
    if (head->x == food_x && head->y == food_y) {
        score += 10;
        generate_food();
    } else {
        // 没吃到食物，删除蛇尾
        SnakeNode *temp = head;
        while (temp->next->next != NULL) {
            temp = temp->next;
        }
        free(temp->next);
        temp->next = NULL;
    }
}

// 释放蛇内存
void free_snake() {
    SnakeNode *current = head;
    while (current != NULL) {
        SnakeNode *next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
}

int main() {
    // 初始化 ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100);  // 设置输入超时（毫秒）

    // 初始化游戏
    init_snake();
    generate_food();

    int ch;
    while (!game_over) {
        draw();

        // 获取用户输入
        ch = getch();

        // 处理方向键（防止反向移动）
        switch (ch) {
            case KEY_UP:
                if (direction != DOWN) direction = UP;
                break;
            case KEY_DOWN:
                if (direction != UP) direction = DOWN;
                break;
            case KEY_LEFT:
                if (direction != RIGHT) direction = LEFT;
                break;
            case KEY_RIGHT:
                if (direction != LEFT) direction = RIGHT;
                break;
            case 'q':
            case 'Q':
                game_over = 1;
                break;
        }

        // 移动蛇
        move_snake();

        // 控制游戏速度
        usleep(100000);  // 100毫秒
    }

    // 清理
    free_snake();
    endwin();

    // 显示最终分数
    printf("游戏结束！最终分数: %d\n", score);

    return 0;
}
