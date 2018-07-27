#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>


#define FIELD_WIDTH 40
#define FIELD_HEIGHT 25
#define MAX_SCORE (FIELD_HEIGHT * FIELD_HEIGHT)

typedef struct _position {
	uint8_t x;
	uint8_t y;
} position;

struct _snake_piece{
	struct _snake_piece *next_piece;
	struct _snake_piece *previous_piece;
	position pos;
};

typedef struct _snake_piece snake_piece;

typedef enum _directions {UP, DOWN, LEFT, RIGHT} directions;

typedef enum _game_states {PLAYING, END} game_states;

typedef struct _apple{
	position pos;
} apple;


void mode_raw(int activer)
{
static struct termios cooked;
static int raw_actif = 0;

if (raw_actif == activer)
    return;

if (activer)
{
    struct termios raw;

    tcgetattr(STDIN_FILENO, &cooked);

    raw = cooked;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}
else
    tcsetattr(STDIN_FILENO, TCSANOW, &cooked);

raw_actif = activer;

}




sem_t input_sem;
pthread_t input_t;
pthread_attr_t thread_attrs;
	
static apple *curr_apple;
directions direction;
game_states game_state;
pthread_mutex_t dir_mutex;

static snake_piece *snake, *head, *tail;

static char* playing_field[FIELD_HEIGHT][FIELD_WIDTH];
/*
static char *head_up_tile = "\x1b[1;33m◼\x1b[0m";
static char *head_down_tile = "\x1b[1;33m◼\x1b[0m";
static char *head_left_tile = "\x1b[1;33m◼\x1b[0m";
static char *head_right_tile = "\x1b[1;33m◼\x1b[0m";
static char *body_tile = "\x1b[1;32m◼\x1b[0m";
static char *apple_tile = "\x1b[1;31mO\x1b[0m";
static char *empty_tile = ".";
*/

static char *head_up_tile = "#";
static char *head_down_tile = "#";
static char *head_left_tile = "#";
static char *head_right_tile = "#";
static char *body_tile = "#";
static char *apple_tile = "O";
static char *empty_tile = ".";


static unsigned int base_wait = 100000;

static int curr_score = 0;
static float speed_factor = 1.0f;

int direction_changed = 0;


void prepare_game();
void prepare_field();
void init_snake();
void make_apple();
void place_apple();
void clear_game();
void clear_snake();
void clear_apple();
int apple_on_snake();
int ate_apple(position next_pos);
int ate_itself(position next_pos);
void update_snake_position(snake_piece *new_head, snake_piece *new_tail);
void draw_field();

void play_game();
void move_snake();
position getNextPosition();

void init_input_handling();
void* process_input();
void stop_input_processing();
int clashing_direction(directions new_dir);
void draw_ui();

void prepare_log();

int main()
{
	
	prepare_game();
	game_state = PLAYING;
	play_game();
	
	clear_game();
	return 0;
}

void prepare_game()
{
	prepare_log();
	
	srand(time(NULL));
	
	direction_changed = 1;
	//printf("Preparing\n");
	init_snake();
	make_apple();
	place_apple();
	prepare_field();
	draw_field();
	
	init_input_handling();
}

void init_snake()
{
	int r;
	
	head = snake = malloc(sizeof(snake_piece));
	tail = malloc(sizeof(snake_piece));	
	
	
	/* we could go on and on through phislosophical discussion about what 
	 * should be the next and what should be the previous piece of a snake
	 * but I randomly chose to do it like this so I will stick with it*/ 
	head->next_piece = tail;
	head->previous_piece = NULL;
	
	tail->previous_piece = head;
	tail->next_piece = NULL;
	
	/* initial position of head should be somewhere away from the edges
	 * so that the tail can fit and so that the player has at least one
	 * tile of space left to maneuver away from a wall should the game 
	 * mode include loss upon wall collision */
	r = rand() % (FIELD_WIDTH - 2) + 1;
	head->pos.x = r;
	r = rand() % (FIELD_HEIGHT - 2) + 1;
	head->pos.y = r;
	
	/* we randomly choose a direction in which the snake starts moving, also
	 * it's how we decide where the tail should be*/
	r = rand() % 4;

	switch (r){
		case 0 :
			direction = UP;
			tail->pos.x = head->pos.x;
			tail->pos.y = head->pos.y + 1;
			break;
		case 1 :
			direction = LEFT;
			tail->pos.x = head->pos.x + 1;
			tail->pos.y = head->pos.y;
			break;
		case 2 :
			direction = DOWN;
			tail->pos.x = head->pos.x;
			tail->pos.y = head->pos.y - 1;
			break;
		case 3 :
			direction = RIGHT;
			tail->pos.x = head->pos.x - 1;
			tail->pos.y = head->pos.y;
			break;
		default :
			printf("Error: Choosing the inital direction of movement "
				"failed\n");
	}
	
}


void clear_game()
{
	clear_snake();
	clear_apple();
	stop_input_processing();
	nocbreak();
	echo();
	endwin();
}

void clear_snake()
{
	snake_piece *curr, *next;
	
	curr = head;
	while (curr != NULL){
		next = curr->next_piece;
		free(curr);
		curr = next;
	}
	
}

void clear_apple()
{
	free(curr_apple);
}

void place_apple()
{
	int r;
	
	do {
		r = rand() % (FIELD_HEIGHT - 2) + 1;
		curr_apple->pos.y = r;
		
		r = rand() % (FIELD_WIDTH - 2) + 1;
		curr_apple->pos.x = r;
	} while (apple_on_snake());
}

/* We don't want the apple to spawn on the snake for practical reasons*/
int apple_on_snake()
{
	snake_piece *curr_piece = head;
	while (curr_piece != NULL) {
		if (curr_piece->pos.x == curr_apple->pos.x &&
			curr_piece->pos.y == curr_apple->pos.y)
			return 1;
		curr_piece = curr_piece->next_piece;
	}
	
	return 0;
}
void make_apple()
{
	curr_apple = malloc(sizeof(apple));
	curr_apple->pos.x = -1;
	curr_apple->pos.y = -1;
}

void prepare_field()
{
	int i, j;
	initscr();
	cbreak();
	noecho();
	for (i = 0; i < FIELD_HEIGHT; i++)
		for (j = 0; j < FIELD_WIDTH; j++)
			playing_field[i][j] = empty_tile;
		
	/* adding the position of the head and the tail on the playing field*/
	switch (direction) {
		case UP :
			playing_field[head->pos.y][head->pos.x] = head_up_tile;
			break;
		case LEFT:
			playing_field[head->pos.y][head->pos.x] = head_left_tile;
			break;
		case DOWN:
			playing_field[head->pos.y][head->pos.x] = head_down_tile;
			break;
		case RIGHT:
			playing_field[head->pos.y][head->pos.x] = head_right_tile;
			break;
			
	}
	playing_field[tail->pos.y][tail->pos.x] = body_tile;
	
	/* adding the position of the apple to the playing field */
	playing_field[curr_apple->pos.y][curr_apple->pos.x] = apple_tile;
}

void play_game()
{	
	char placeholder;
	int count = 0, sem_val;
	
	draw_field();
	draw_ui();
	getch();
	while(game_state != END){
		
		move_snake();
		draw_field();
		draw_ui();
		refresh();
		
		sem_getvalue(&input_sem, &sem_val);
		if(sem_val < 1){
			mvprintw(FIELD_HEIGHT+5, 0, "the value of semaphore right before post is %d", sem_val);
			sem_post(&input_sem);
		}
		else 
			mvprintw(FIELD_HEIGHT, 2, "%d", sem_val);
		
		usleep((unsigned int)(base_wait));
	}
}

void move_snake()
{
	position next_pos = getNextPosition();
	snake_piece *new_head = NULL, *new_tail = NULL;
	int sem_val;
	
	
	if (ate_itself(next_pos)){
		//printf("MOTHAFUCKA\n");
		//end the game and print out the message optionally
		//for now this is the way we exit
		mvprintw(FIELD_HEIGHT/2-1, FIELD_WIDTH/2 - 8, "               ");
		mvprintw(FIELD_HEIGHT/2,   FIELD_WIDTH/2 - 8, "   GAME OVER   ");
		mvprintw(FIELD_HEIGHT/2+1, FIELD_WIDTH/2 - 8, "               ");
		refresh();
		getch();
		nocbreak();
		echo();
		endwin();
		exit(0);
	}
	else {
		//Lengthen the snake one tile
		new_head = malloc(sizeof(snake_piece));
		new_head->pos.x = next_pos.x;
		new_head->pos.y = next_pos.y;
		
		new_head->next_piece = head;
		new_head->previous_piece = NULL;
		head->previous_piece = new_head;
		head = new_head;
		
		//and if it didn't eat anything move up its tail
		if(!ate_apple(next_pos)){
			new_tail = tail->previous_piece;
		} else	{
			playing_field[curr_apple->pos.y][curr_apple->pos.x] = empty_tile;
			place_apple();
			playing_field[curr_apple->pos.y][curr_apple->pos.x] = apple_tile;
			curr_score++;
			base_wait -= 1000;
			
		}
	}
	update_snake_position(new_head, new_tail);
	//if we moved the tail and updated how the snake looks we can
	//free up the old tail piece
	if(new_tail != NULL){
		free(new_tail->next_piece);
		new_tail->next_piece = NULL;
		tail = new_tail;
	}
}

int ate_apple(position next_pos)
{
	return (next_pos.x == curr_apple->pos.x && next_pos.y == curr_apple->pos.y);
}

position getNextPosition()
{
	position next;
	directions dir;
	
	pthread_mutex_lock(&dir_mutex);
	dir = direction;
	pthread_mutex_unlock(&dir_mutex);
	
	switch (dir) {
		case UP :
			next.y = (head->pos.y - 1) < 0 ? (FIELD_HEIGHT - 1) : head->pos.y - 1;
			next.x = head->pos.x;
			break;
		case LEFT:
			next.y = head->pos.y;
			next.x = (head->pos.x - 1) < 0 ? (FIELD_WIDTH - 1) : head->pos.x -1;
			break;
		case DOWN:
			next.y = (head->pos.y + 1) % FIELD_HEIGHT;
			next.x = head->pos.x;
			break;
		case RIGHT:
			next.y = head->pos.y;
			next.x = (head->pos.x + 1) % FIELD_WIDTH;
			break;
		default :
			printf("Something has gone wrong with fetching the next position\n");
	}
	return next;
}

int ate_itself(position next_pos)
{
	snake_piece *curr_piece = head;
	
	while (curr_piece != NULL){
		if(curr_piece->pos.x == next_pos.x && 
			curr_piece->pos.y == next_pos.y)
			return 1;
		curr_piece = curr_piece->next_piece;
	}
	return 0;
}

void update_snake_position(snake_piece *new_head, snake_piece *new_tail)
{
	//for now any tile will suffice, they are all squares after all
	playing_field[new_head->pos.y][new_head->pos.x] = head_down_tile;
	playing_field[new_head->next_piece->pos.y][new_head->next_piece->pos.x] = body_tile;
	
	if(new_tail != NULL)
		playing_field[new_tail->next_piece->pos.y][new_tail->next_piece->pos.x] = empty_tile;
}

void draw_field()
{
	int i, j;
		
	for (i = 0; i < FIELD_HEIGHT; i++){
		for (j = 0;  j < FIELD_WIDTH; j++)
			mvprintw(i, j, "%s", playing_field[i][j]);
			//printf("%s", playing_field[i][j]);
			//write(1, playing_field[i][j], strlen(playing_field[i][j]));
	}
}

void init_input_handling()
{
	int sem_val;
	
	sem_init(&input_sem, 0, 0);
	
	pthread_mutex_init(&dir_mutex, NULL);
	pthread_create(&input_t, NULL, process_input, NULL);
}

void* process_input()
{
	int i, sem_val;
	char d;
	directions new_dir;
	sem_wait(&input_sem);
	while(1){
		d = getch();
		sem_wait(&input_sem);
		sem_getvalue(&input_sem, &sem_val);
		mvprintw(FIELD_HEIGHT+4, 0, "we exit the wait and the sem value is %d\n", sem_val);
		refresh();
		if(d != -1){
			switch(d){
				case 'w':
					new_dir = UP;
					break;
				case 'a' :
					new_dir = LEFT;
					break;
				case 's' :
					new_dir = DOWN;
					break;
				case 'd' :
					new_dir = RIGHT;
					break;
				default :
					break;
			}
			if(!clashing_direction(new_dir)){
				pthread_mutex_lock(&dir_mutex);
				direction = new_dir;
				pthread_mutex_unlock(&dir_mutex);
				draw_ui();
			}
		}
	}
}

void stop_input_processing()
{
	pthread_join(input_t, NULL);
	pthread_mutex_unlock(&dir_mutex);
	pthread_mutex_destroy(&dir_mutex);
	
}

int clashing_direction(directions new_dir)
{
	pthread_mutex_lock(&dir_mutex);
	directions old_dir = direction;
	pthread_mutex_unlock(&dir_mutex);
	
	if((new_dir == UP) && (old_dir == DOWN))
		return 1;
	if((new_dir == DOWN) && (old_dir == UP))
		return 1;
	if((new_dir == LEFT) && (old_dir == RIGHT))
		return 1;
	if((new_dir == RIGHT) && (old_dir == LEFT))
		return 1;
	
	return 0;
}

void draw_ui()
{
	char d;
	int sem_val;
	
	sem_getvalue(&input_sem, &sem_val);
	
	switch(direction){
		case UP:
			d = 'w';
			break;
		case DOWN :
			d = 's';
			break;
		case RIGHT :
			d = 'd';
			break;
		case LEFT :
			d = 'a';
			break;
		default :
			d = '?';
			break;
	}
	
	mvprintw(FIELD_HEIGHT, 0, "%c, and semaphore at time %d", d, sem_val);
	refresh();
}

void prepare_log()
{
	
}