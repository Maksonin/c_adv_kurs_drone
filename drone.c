#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <curses.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#define MIN_Y    3
#define PLAYERS  1

double DELAY = 0.1;
int SEED_NUMBER = 10;

enum { LEFT=1, UP, RIGHT, DOWN, STOP_GAME=KEY_F(10), CONTROLS=4, PAUSE_GAME='p', AUTO_MOVE='m' };
enum {	MAX_HARVEST_SIZE=6, 
		START_HARVEST_SIZE=6, 
		MAX_FOOD_SIZE=10, 
		FOOD_EXPIRE_SECONDS=0,
};
enum {
    HARVESTED,
    CONFLICT
};

// Структура для хранения кодов управления дроном
struct control_buttons
{
    int down;
    int up;
    int left;
    int right;
} control_buttons;

struct control_buttons default_controls[CONTROLS] = {   {KEY_DOWN, KEY_UP, KEY_LEFT, KEY_RIGHT},
                                                        {'s', 'w', 'a', 'd'},
                                                        {'S', 'W', 'A', 'D'},
                                                        {0xFFFFFFEB, 0xFFFFFFE6, 0xFFFFFFE4, 0xFFFFFFA2},
                                                        {0xFFFFFF9B, 0xFFFFFF96, 0xFFFFFF94, 0xFFFFFF82}
};

/*
 Структура drone содержит в себе
 x,y - координаты текущей позиции
 direction - направление движения
 cartSize - количество телег
 *harvest -  ссылка на телеги
 */
typedef struct drone_t
{
    int x;
    int y;
    int harvestStartX;
    int harvestStartY;
    int direction;
    uint8_t speed;
    _Bool autoMove;
    size_t cartSize;
    size_t loadedCart;
    struct harvest_t *harvest;
    struct control_buttons* controls;
} drone_t;

/*
* Массив состоящий из координат x,y (описывает телеги)
*/
typedef struct harvest_t
{
    int x;
    int y;
    _Bool full;
} harvest_t;

/*
* Массив состоящий из координат x,y (описывает овощи на поле)
*/
struct food
{
    int x;
    int y;
    time_t put_time;
    char point;
    uint8_t enable;
} food[MAX_FOOD_SIZE];

/*
* Структура описания зоны разгрузки
*/
struct home
{
    int x;
    int y;
} home;

/* 
* Инициализация головы дрона 
*/
void initHead(drone_t *head, int x, int y)
{
    head->x = x;
    head->y = y;
    head->direction = RIGHT;
}

/* 
* Инициализация телег 
*/
void initHarvest(harvest_t t[], size_t size)
{
    struct harvest_t init_t={0,0,0};
    for(size_t i=0; i<size; i++)
    {
        t[i]=init_t;
    }
}

/* 
* Инициализация комплекса дрон + телеги 
*/
void initDrone(drone_t *head[], size_t size, int x, int y,int i)
{
    head[i]    = (drone_t*)malloc(sizeof(drone_t));
	harvest_t*  harvest  = (harvest_t*) malloc(MAX_HARVEST_SIZE*sizeof(harvest_t));
    initHarvest(harvest, MAX_HARVEST_SIZE);
    initHead(head[i], x, y);
    head[i]->harvest  = harvest; // прикрепляем к голове телеги
    head[i]->cartSize    = size+1;
    head[i]->loadedCart  = 0;
	head[i]->autoMove = false;
    head[i]->controls = default_controls;
    //~ head->controls = default_controls[1];
}

/* 
* Инициализация еды 
*/
void initFood(struct food f[], size_t size)
{
    struct food init = {0,0,0,0,0};
    for(size_t i=0; i<size; i++)
    {
        f[i] = init;
    }
}

/*
* Функция отрисовки склада
*/
void drowHome(struct home *base){
    mvprintw(base->y-1, base->x, "#");
    mvprintw(base->y, base->x-2, "> ");
    mvprintw(base->y, base->x,   "о");
    mvprintw(base->y+1, base->x, "#");
    mvprintw(base->y, base->x+1, " <");
}

/*
* Инициализация базы выгрузки
*/
void initHome(struct home *base, int x, int y) {
    base->x = x;
    base->y = y;

    //mvprintw(1, 90, "home - %d - %d ", base->y, base->x);

    int max_x = 0, max_y = 0;
    getmaxyx(stdscr, max_y, max_x);
    if((x < max_x)&&(y < max_y)){
        drowHome(base);
    }
}

/*
 Движение дрона с учетом текущего направления движения
 */
void go(struct drone_t *head)
{
    char ch = '@';
    int max_x=0, max_y=0;
    getmaxyx(stdscr, max_y, max_x); // macro - размер терминала
    mvprintw(head->y, head->x, " "); // очищаем один символ
    head->harvestStartX = head->x;
    head->harvestStartY = head->y;
    switch (head->direction)
    {
        case LEFT:
            if(head->x == 0) // проверка на границу
                head->direction = 0; 
            else if(head->x > 0) {
                head->direction = LEFT;
				mvprintw(head->y, --(head->x), "%c", ch);
            }
        break;
        case RIGHT:
            if(head->x == max_x - 1) // проверка на границу
                head->direction = 0;
            else if(head->x < max_x){
                head->direction = RIGHT;
				mvprintw(head->y, ++(head->x), "%c", ch);
            }
        break;
        case UP:
            if(head->y == MIN_Y) // проверка на границу
                head->direction = 0;
			else if(head->y > MIN_Y){
                head->direction = UP;
				mvprintw(--(head->y), head->x, "%c", ch);
            }
        break;
        case DOWN:
            if(head->y == max_y - 1) // проверка на границу
                head->direction = 0;
            else if(head->y < max_y){
                head->direction = DOWN;
				mvprintw(++(head->y), head->x, "%c", ch);
            }
        break;
        default:
        break;
    }
    if(!(head->direction))
        mvprintw(head->y, head->x, "%c", ch);
    
    refresh();
}

/*
 Движение хвоста с учетом движения головы
 */
void goHarvest(struct drone_t *head)
{
    char ch = '_';
    char loadch = '*';
    if(!(head->direction))
        return;

    for(size_t i = head->cartSize-1; i > 0; i--) {
        head->harvest[i].x = head->harvest[i-1].x;
        head->harvest[i].y = head->harvest[i-1].y;

        if(head->harvest[i].full)
            mvprintw(head->harvest[i].y, head->harvest[i].x, "%c", loadch);
        else
            mvprintw(head->harvest[i].y, head->harvest[i].x, "%c", ch);
    }
    mvprintw(head->harvest[head->cartSize-1].y, head->harvest[head->cartSize-1].x, " ");
    
    head->harvest[0].x = head->harvestStartX;
    head->harvest[0].y = head->harvestStartY;
    if(head->harvest[0].full)
        mvprintw(head->harvest[0].y, head->harvest[0].x, "%c", loadch);
    else
        mvprintw(head->harvest[0].y, head->harvest[0].x, "%c", ch);
}

/*
* Выбор направления движения в соответствии с нажатой кнопкой
*/
void changeDirection(struct drone_t* drone, const int32_t key)
{
    for (int i = 0; i < CONTROLS; i++)
    {
        if (key == drone->controls[i].down)
            drone->direction = DOWN;
        else if (key == drone->controls[i].up)
            drone->direction = UP;
        else if (key == drone->controls[i].right)
            drone->direction = RIGHT;
        else if (key == drone->controls[i].left)
            drone->direction = LEFT;
    }
}

/*
* Проверка текущего направления движения дрона в соответствии с нажатой кнопкой
* Например: если движение влево и нажата кнопка вправо, то возвращается 0 -> запрет обратного движения
*/
int checkDirection(drone_t* drone, int32_t key)
{
    for (int i = 0; i < CONTROLS; i++)
        if((drone->controls[i].down  == key && drone->direction==UP)    ||
           (drone->controls[i].up    == key && drone->direction==DOWN)  ||
           (drone->controls[i].left  == key && drone->direction==RIGHT) ||
           (drone->controls[i].right == key && drone->direction==LEFT))
        {
            return 0;
        }
    return 1;
}

/*
 Обновить/разместить текущее зерно на поле
 */
void putFoodSeed(struct food *fp)
{
    int max_x=0, max_y=0;
    char spoint[2] = {0};
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(fp->y, fp->x, " ");
    fp->x = rand() % (max_x - 1);
    fp->y = rand() % (max_y - 2) + 1; //Не занимаем верхнюю строку
    fp->put_time = time(NULL);
    fp->point = '$';
    fp->enable = 1;
    spoint[0] = fp->point;
    mvprintw(fp->y, fp->x, "%s", spoint);
}

/*
 Разместить еду на поле
 */
void putFood(struct food f[], size_t number_seeds)
{
    for(size_t i=0; i<number_seeds; i++)
    {
        putFoodSeed(&f[i]);
    }
}

/*
* Отрисовка массива еды
*/
void refreshFood(struct food f[], int nfood)
{
    for(size_t i=0; i<nfood; i++)
    {
            if( !f[i].enable ) // если точка еды активна
            {
                putFoodSeed(&f[i]); // отрисовываем
            }
    }
}

/*
*
*/
void repairSeed(struct food f[], size_t nfood, struct drone_t *head)
{
    for( size_t i=0; i<head->cartSize; i++ )
        for( size_t j=0; j<nfood; j++ )
        {
		/* Если хвост совпадает с зерном */
            if( f[j].x == head->harvest[i].x && f[j].y == head->harvest[i].y && f[i].enable )
            {
                //mvprintw(1, 0, "Repair harvest seed %u - %d - %d  ", j, f[j].y, f[j].x);
                //putFoodSeed(&f[j]);
                //mvprintw(f[j].y, f[j].x, "$");
            }
        }
    for( size_t i=0; i<nfood; i++ )
        for( size_t j=0; j<nfood; j++ )
        {
		/* Если два зерна на одной точке */
            if( i!=j && f[i].enable && f[j].enable && f[j].x == f[i].x && f[j].y == f[i].y && f[i].enable )
            {
                mvprintw(1, 0, "Repair same seed %u",j);
                putFoodSeed(&f[j]);
            }
        }
}

/*
* Обработка процесса совмещения дрона с тележками и еды
*/
_Bool haveEat(struct drone_t *head, struct food f[])
{
    for(size_t i=0; i<MAX_FOOD_SIZE; i++)
        if( f[i].enable && head->x == f[i].x && head->y == f[i].y  )
        {
            if(head->loadedCart == MAX_HARVEST_SIZE){
                return 0;
            }
            else {  
                f[i].enable = 0;
                f[i].point = '-';
                return 1;
            }
        }
    return 0;
}

/*
* Обработка прохождения через склад
*/
void returnHome(struct drone_t *head, struct home *base)
{
    if(head->loadedCart > 0){ // если хоть одна тележка загружена
        for(int i = 0; i < MAX_HARVEST_SIZE; i++){ // цикл для проверки совмещения тележки с базой
            if((head->harvest[i].x == base->x) && (head->harvest[i].y == base->y) && (head->harvest[i].full != false)){
                head->loadedCart--;
                head->harvest[i].full = false;
                SEED_NUMBER--;
            }
        }
    }
    else drowHome(base);
    mvprintw(base->y, base->x,"-"); // отрисовываем базу
    printLevel(head);
}


/*
 Добавление телеги
 */
void addharvest(struct drone_t *head)
{
    if(head == NULL || head->loadedCart >= MAX_HARVEST_SIZE)
    {
        mvprintw(0, 0, "Can't add harvest");
        return;
    }
    head->loadedCart++;
    head->harvest[head->loadedCart - 1].full = true;
}

/*
* Определение дистанции до еды
*/
int distance(const drone_t drone, const struct food food) {   // вычисляет количество ходов до еды
    return (abs(drone.x - food.x) + abs(drone.y - food.y));
}

/*
*
*/
_Bool findHarvestConflict(drone_t *drone, struct food food[], int newDirection){
/***/
    char *st[] = {
        "   -   ",
        " Left  ",
        " Up    ",
        " Right ",
        " Down  ",
    };
    mvprintw(1, 100, " %s ", st[newDirection]);
    /***/
    for(int i = 0; i < MAX_HARVEST_SIZE; i++){
        if(newDirection == UP){
            if((drone->y - 1 == drone->harvest[i].y) && (drone->x == drone->harvest[i].x )){
                return 0;
            }
        }
        else if(newDirection == DOWN){
            if((drone->y + 1 == drone->harvest[i].y) && (drone->x == drone->harvest[i].x )){
                return 0;
            }
        }
        else if(newDirection == RIGHT){
            if((drone->y == drone->harvest[i].y) && (drone->x + 1 == drone->harvest[i].x )){
                return 0;
            }
        }
        else if(newDirection == LEFT){
            if((drone->y == drone->harvest[i].y) && (drone->x - 1 == drone->harvest[i].x )){
                return 0;
            }
        }
    }
    //drone->direction = newDirection;
    return 1;
}

/*
* Автоматическое следование дрона на урожай
*/
void autoChangeDirection(drone_t *drone, struct food food[], int foodSize) {
    int pointer = 0;
    int newDirection = 0;
    for (int i = 0; i < foodSize; i++) {   // ищем ближайшую еду
        if(food[i].enable)
            pointer = (distance(*drone, food[i]) < distance(*drone, food[pointer])) ? i : pointer;
    }

    // выделение еды к которой стремится дрон
    food[pointer].point = 'O';
    mvprintw(food[pointer].y,food[pointer].x, "%c", food[pointer].point);
    
    // если дрон в стороне от еды
    if ((drone->direction == RIGHT || drone->direction == LEFT) && (drone->y != food[pointer].y)) {  // горизонтальное движение
        //drone->direction = (food[pointer].y > drone->y) ? DOWN : UP;
        newDirection = (food[pointer].y > drone->y) ? DOWN : UP;
        if(findHarvestConflict(drone, food, newDirection)){ // проверка на наличие хвоста по направлению движения
            drone->direction = newDirection;
        }
    } 
    else if ((drone->direction == DOWN || drone->direction == UP) && (drone->x != food[pointer].x)) {  // вертикальное движение
        //drone->direction = (food[pointer].x > drone->x) ? RIGHT : LEFT;
        newDirection = (food[pointer].x > drone->x) ? RIGHT : LEFT;
        if(findHarvestConflict(drone, food, newDirection)){ // проверка на наличие хвоста по направлению движения
            drone->direction = newDirection;
        }
    }

    // если дрон на одной линии с едой
    if(drone->x == food[pointer].x) {
        if(((drone->y > food[pointer].y) && (drone->direction == DOWN)) || ((drone->y < food[pointer].y) && (drone->direction == UP))){
            newDirection = LEFT;
            if(!findHarvestConflict(drone, food, newDirection))
               drone->direction = newDirection;
            else 
                drone->direction = RIGHT;
        }
    }
    if(drone->y == food[pointer].y) {
        if(((drone->x > food[pointer].x) && (drone->direction == RIGHT)) || ((drone->x < food[pointer].x) && (drone->direction == LEFT))){
            newDirection = UP;
            if(!findHarvestConflict(drone, food, newDirection))
               drone->direction = newDirection;
            else 
                drone->direction = DOWN;
        }
    }
}

/*
* Обновление состояния
*/
void update(drone_t *head, struct food f[], int key)
{
    // вывод координат головы, код кнопки, направления движения, количества оставшейся еды
	mvprintw(1, 40, "  x - %d, y - %d, key - %d, dir - %d, food - %d auto - %d  ", head->x, head->y, key, head->direction, SEED_NUMBER, head->autoMove); // вывод координат дрона

    if (checkDirection(head, key))
    {
        changeDirection(head, key);
    }
    if(head->autoMove)
        autoChangeDirection(head,f,SEED_NUMBER);

    if(!(head->direction)){
        mvprintw(1, 30, " Border ");
        return;
    }
    else {
        mvprintw(1, 30, "        ");
        /*  */
        if(head->autoMove){
            DELAY = 0.1;
            go(head);
            goHarvest(head);
        } else {
            if(key > -1){
                DELAY = 0.03;
                go(head);
                goHarvest(head);
            }
	    }
    }

    // тестовый вывод состояния тележек
    //for(int i = 0; i < MAX_HARVEST_SIZE; i++){
    //    mvprintw(3 + i, 80, " %d - %d - %d - %d ", i, head->harvest[i].x, head->harvest[i].y, head->harvest[i].full);
    //}
    // тестовый вывод состояния тележек
    // тестовый вывод состояния еды
    for(int i = 0; i < SEED_NUMBER; i++){
       mvprintw(3 + i, 80, " %d - %d - %c - %d ", food[i].x, food[i].y, food[i].point, food[i].enable);
    }
    // тестовый вывод состояния еды

    returnHome(head, &home); // проверка на возврат домой

    if (haveEat(head,food))
    {
        addharvest(head);
        printLevel(head);
        //~ DELAY -= 0.009;
    }
}

/*
* Функция временной паузы в работе
*/
void pause(void)
{
    int max_x = 0, max_y = 0;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(max_y / 2, max_x / 2 - 5, " Press P to continue work ");
    while (getch() != PAUSE_GAME)
    {}
    mvprintw(max_y / 2, max_x / 2 - 5, "                          ");
}

/*
* Проверка на аварию
*/
_Bool isCrush(drone_t * drone)
{
    for(size_t i=1; i<drone->cartSize; i++)
        if(drone->x == drone->harvest[i].x && drone->y == drone->harvest[i].y)
            return 1;
    return 0;
}

/*
* Вывод количества собранной еды
*/
void printLevel(struct drone_t *head)
{
    int max_x = 0, max_y = 0;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(0, max_x - 20, "HARVEST: %d / %d", head->loadedCart, MAX_HARVEST_SIZE);
}

/*
* Вывод сообщения об окончании работы
* 0 - все собрано
* 1 - конфликт
*/
void printExit(int status)
{
    int max_x = 0, max_y = 0;
    getmaxyx(stdscr, max_y, max_x);
    char *st[] = {
        " The harvest is harvested. ",
        " Conflict! ",
    };
    mvprintw(max_y / 2, max_x / 2 - 20, "  %s Press Whitespace to exit.  ", st[status]);
}

int main()
{
	drone_t* drones[PLAYERS];
    for (int i = 0; i < PLAYERS; i++)
        initDrone(drones, START_HARVEST_SIZE, 10+i*10, 10+i*10, i);

    drones[0]->controls = default_controls;
    // drones[1]->controls = pleer2_controls;
    initscr();
    keypad(stdscr, TRUE); // Включаем F1, F2, стрелки и т.д.
    raw();                // Откдючаем line buffering
    noecho();            // Отключаем echo() режим при вызове getch
    curs_set(FALSE);    //Отключаем курсор
    mvprintw(0, 0," Press WASD or arrows for move,'M' for on/off automove, 'p' for pause, 'F10' for EXIT");
    timeout(0);    //Отключаем таймаут после нажатия клавиши в цикле
    initFood(food, MAX_FOOD_SIZE);
    putFood(food, SEED_NUMBER);// Кладем зерна

    initHome(&home, 15, 28); // инициализация склада

    /* Отрисовка линии верхней границы поля */
    for(int i = 0; i < getmaxx(stdscr); i++)
        mvprintw(2, i,"-");

    int key_pressed=0;
    int isFinish = 0;

    /*  */
    char ch = '@';
    mvprintw(drones[0]->y, drones[0]->x, "%c", ch);
    /*  */
    ch = '_';
    for(int i = 0; i < drones[0]->cartSize - 1; i++)
    {
        drones[0]->harvest[i].y = drones[0]->y;
        drones[0]->harvest[i].x = (drones[0]->x) - 1 - i;
        mvprintw(drones[0]->harvest[i].y, drones[0]->harvest[i].x, "%c", ch);
    }
    /*  */
    
    while( key_pressed != STOP_GAME && !isFinish)//
    {
        clock_t begin = clock();
        key_pressed = getch(); // Считываем клавишу

        // включение/выключение автоматического движения дрона
        if (key_pressed == AUTO_MOVE) 
        {
            if(drones[0]->autoMove)
                drones[0]->autoMove = false;
            else
                drones[0]->autoMove = true;
        }

        // обработка движения
        for (int i = 0; i < PLAYERS; i++)
        {
            update(drones[i], food, key_pressed);

            if(!SEED_NUMBER){ // если еда на поле закончилась
                printExit(HARVESTED);
                isFinish = 1;
            }
            
            if(isCrush(drones[i]))
            {
                printExit(CONFLICT);
                isFinish = 1;
            }
            repairSeed(food, SEED_NUMBER, drones[i]);
        }

        // обработка паузы
        if (key_pressed == PAUSE_GAME)
        {
            pause();
        }
        refresh();//Обновление экрана, вывели кадр анимации
		
        while ((double)(clock() - begin) / CLOCKS_PER_SEC < DELAY)
        {}
    }

    key_pressed=0;
    while( key_pressed != ' ' )//
    {
        key_pressed = getch(); // Считываем клавишу
    }

    for (int i = 0; i < PLAYERS; i++)
    {
        free(drones[i]->harvest);
        free(drones[i]);
    }


    endwin(); // Завершаем режим curses mod
    return 0;
}

