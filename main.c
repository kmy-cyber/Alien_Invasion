#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define DELAY 30000 // Tiempo de espera entre actualizaciones en microsegundos
#define MAX_PROJECTILES 5 // Máximo número de proyectiles que puede tener el jugador
#define MAX_ENEMIES 5 // Máximo número de enemigos en el juego
#define SPEED_LOW_ENEMIES 10 // Velocidad de movimiento de los enemigos

typedef struct {
    int x, y;
} Position;
typedef struct {
    Position pos;
    int active; // Indica si el proyectil está activo o no
} Projectile;

typedef struct {
    Position pos;
    int active; // Indica si el enemigo está activo o no
    int type; // Tipo de enemigo (para variedad visual)
} Enemy;

// Variables globales
Position player; // Posición del jugador
Projectile projectiles[MAX_PROJECTILES]; // Array de proyectiles
Enemy enemies[MAX_ENEMIES]; // Array de enemigos
int running = 1; // Variable para controlar el estado de ejecución del juego
int score = 0; // Puntuación del jugador
int hp = 3; // Vida del jugador
int high_score = 0; // Mejor puntuación alcanzada
int state = 0; // Estado del juego (0: inicio, 1: jugando, 2: fin del juego)
int player_direction = 0;
int shooting = 0;
pthread_mutex_t mutex; // Mutex para sincronizar acceso a recursos compartidos

// Funciones del juego
void init_game(); // Inicializa el juego al iniciar una nueva partida
void *game_loop(void *arg); // Bucle principal del juego
void *input_handler(void *arg); // Manejador de entrada para capturar eventos del usuario
void *move_handler(void *arg);
void *shoot_handler(void *arg);
void move_player(int direction); // Mueve al jugador según la dirección indicada
void shoot(); // Dispara un proyectil desde la posición del jugador
void update_projectiles(); // Actualiza la posición de los proyectiles
void update_enemies(); // Actualiza la posición de los enemigos
void check_collisions(); // Verifica colisiones entre proyectiles y enemigos
void draw_borders(); // Dibuja los bordes de la pantalla
void draw_ship(int x, int y); // Dibuja el barco del jugador
void draw_enemy(int x, int y, int type); // Dibuja un enemigo en la pantalla
int check_collision(Position pos, Position *parts, int size); // Comprueba si hay colisión entre dos objetos
int check_collision_enemies(Position *ship_parts, Position *parts, int size); // Comprueba si hay colisión entre el barco y los enemigos
void update_score(int type); // Actualiza la puntuación basada en el tipo de enemigo derrotado
void draw_start_screen(); // Dibuja la pantalla de inicio del juego
void draw_game_over_screen(); // Dibuja la pantalla de fin del juego

// Función principal del programa
int main() {
    srand(time(NULL)); // Inicializa la semilla para generar números aleatorios
    initscr(); // Inicia el modo ncurses
    noecho(); // Desactiva el eco de teclado
    curs_set(FALSE); // Oculta el cursor
    timeout(0); // Configura getch para ser no bloqueante
    start_color();//iniciar color
    
    // Definir pares de colores
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);

    player_direction = 0;
    shooting = 0;

    pthread_t game_thread, input_thread, move_thread, shoot_thread; // Declara los identificadores de los hilos para el juego y el manejo de entrada
    pthread_mutex_init(&mutex, NULL); // Inicializa el mutex para sincronización

    // Crea los hilos para el bucle del juego y el manejo de entrada
    pthread_create(&game_thread, NULL, game_loop, NULL);
    pthread_create(&input_thread, NULL, input_handler, NULL);
    pthread_create(&move_thread, NULL, move_handler, NULL);
    pthread_create(&shoot_thread, NULL, shoot_handler, NULL);

    // Espera a que los hilos terminen antes de continuar
    pthread_join(game_thread, NULL);
    pthread_join(input_thread, NULL);
    pthread_join(move_thread, NULL);
    pthread_join(shoot_thread, NULL);

    endwin();// Finaliza el modo ncurses
    pthread_mutex_destroy(&mutex);// Destruye el mutex

    return 0;
}

// Inicializa el juego, reseteando variables y posicionando al jugador y elementos en sus estados iniciales
void init_game() {
    player.x = COLS / 2; // Coloca al jugador en el centro horizontalmente
    player.y = LINES - 9; // Coloca al jugador cerca del borde inferior
    score = 0; // Resetea la puntuación
    hp = 3; // Resetea la vida del jugador
    
    // Desactiva todos los proyectiles y enemigos al inicio de una nueva partida
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = 0;
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
    }
}

// Bucle principal del juego, controla el estado del juego y actualiza la pantalla según el estado actual
void *game_loop(void *arg) {
    int move_enemy = 0; //controla el mover de los enemigos

    while (running) {
        pthread_mutex_lock(&mutex); //Bloquea mutex para acceso seeguro a las variables
        // Dependiendo del estado, muestra la pantalla de inicio, actualiza el juego o la pantalla de fin de juego
        if (state == 0) {
            draw_start_screen();
        } else if (state == 1) {
            update_projectiles();// Actualiza proyectiles

            //Actualiza enemigos si es necesario
            if (!move_enemy) {
                update_enemies();
            }

            check_collisions();//verifica las colisiones

            //Verificar si hay que actualizar la puntuacion mas alta
            if (hp <= 0) {
                state = 2;
                if (score > high_score) {
                    high_score = score;
                }
            }

            clear();// Limpia la pantalla antes de dibujar
            draw_borders();// Dibuja los bordes
            draw_ship(player.x, player.y);// Dibuja al jugador

            //Imprimir
            mvprintw(1, 2, "HP: %d", hp);
            mvprintw(1, 10, "Score: %d", score);
            mvprintw(1, 20, "High Score: %d", high_score);

            for (int i = 0; i < MAX_PROJECTILES; i++) {
                if (projectiles[i].active) {
                    mvprintw(projectiles[i].pos.y, projectiles[i].pos.x, "|");
                }
            }
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].active) {
                    draw_enemy(enemies[i].pos.x, enemies[i].pos.y, enemies[i].type);
                }
            }
            refresh();// Actualiza la pantalla con los cambios
        } else if (state == 2) {
            draw_game_over_screen(); 
        }

        pthread_mutex_unlock(&mutex);//Desbloquea el mutex

        usleep(DELAY);//espera el proximo ciclo
        move_enemy = (move_enemy + 1) % SPEED_LOW_ENEMIES;// Controla la velocidad de los enemigos
    }
    return NULL;
}

//Maneja la entrada del usuario para controlar el juego
void *input_handler(void *arg) {
    int ch;
    while (running) {
        ch = getch(); //obtiene la entrada del usuario
        pthread_mutex_lock(&mutex); //Bloquea el mutex

        //Cambia el estado del juego segun la entrada
        if (state == 0) {
            if (ch == 'n') {
                state = 1;
                init_game();
            } else if (ch == 'q') {
                running = 0;
            }
        } else if (state == 1) {
            switch (ch) {
                case 'a':
                case KEY_LEFT:
                    player_direction = -1;
                    break;
                case 'd':
                case KEY_RIGHT:
                    player_direction = 1;
                    break;
                case 's': // Detener movimiento
                    player_direction = 0;
                    break;
                case ' ':
                    shooting = 1;
                    break;
                case 'f': // Detener disparo
                    shooting = 0;
                    break;
                case 'q':
                    running = 0;
                    break;
            }
        } else if (state == 2) {
            if (ch == 'r') {
                state = 0;
            } else if (ch == 'q') {
                running = 0;
            }
        }

        pthread_mutex_unlock(&mutex);//Desblqouea el mutex
    }
    return NULL;
}

void *move_handler(void *arg) {
    while (running) {
        pthread_mutex_lock(&mutex);
        if (player_direction != 0) {
            move_player(player_direction);
        }
        pthread_mutex_unlock(&mutex);
        usleep(50000); // Controla la velocidad del movimiento (50 ms)
    }
    return NULL;
}

void *shoot_handler(void *arg) {
    while (running) {
        pthread_mutex_lock(&mutex);
        if (shooting) {
            shoot();
        }
        pthread_mutex_unlock(&mutex);
        usleep(1000); // Controla la velocidad del disparo (100 ms)
    }
    return NULL;
}

// Mueve al jugador según la dirección ingresada por el usuario
void move_player(int direction) {
    if (direction == -1 && player.x > 2) {
        player.x--;
    } else if (direction == 1 && player.x < COLS - 3) {
        player.x++;
    }
}

//Dispara un proyectil desde la posicion del jugador
void shoot() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].pos.x = player.x;
            projectiles[i].pos.y = player.y - 1;
            projectiles[i].active = 1;
            break;
        }
    }
}

//Actualiza las posiciones de los proyectiles activos
void update_projectiles() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) { // Mueve el proyectil hacia arriba y lo desactiva si sale de pantalla
            projectiles[i].pos.y--;
            if (projectiles[i].pos.y < 3) {
                projectiles[i].active = 0;
            }
        }
    }
}

// Actualiza las posiciones y estados de los enemigos
void update_enemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            enemies[i].pos.y++;
            if (enemies[i].pos.y >= LINES - 1) {
                enemies[i].active = 0;
            }
        } else {
            if (rand() % 100 < 5) {
                enemies[i].pos.x = 2 + rand() % (COLS - 4);
                enemies[i].pos.y = 3;
                enemies[i].active = 1;
                enemies[i].type = rand() % 3; // 3 tipos de enemigos
            }
        }
    }
}

// Verifica colisiones entre proyectiles y enemigos, y entre el jugador y enemigos
void check_collisions() {
    Position ship_parts[] = {// Define las partes del jugador para colisiones
        {player.x, player.y},
        {player.x - 1, player.y + 1},
        {player.x + 1, player.y + 1},
        {player.x - 1, player.y + 2},
        {player.x + 1, player.y + 2}
    };

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {// Comprueba cada proyectil contra todos los enemigos
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (enemies[j].active) {
                    Position enemy_parts[] = {
                        {enemies[j].pos.x, enemies[j].pos.y},
                        {enemies[j].pos.x - 1, enemies[j].pos.y + 1},
                        {enemies[j].pos.x + 1, enemies[j].pos.y + 1},
                        {enemies[j].pos.x - 2, enemies[j].pos.y + 2},
                        {enemies[j].pos.x + 2, enemies[j].pos.y + 2}
                    };

                    if (check_collision(projectiles[i].pos, enemy_parts, 5)) {// Comprueba colision entre jugador y enemigos
                        projectiles[i].active = 0;
                        update_score(enemies[j].type);// Suma puntos por tipo de enemigo derrotado
                        enemies[j].active = 0;// Desactiva el enemigo golpeado
                        break;
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            Position enemy_parts[] = {
                {enemies[i].pos.x, enemies[i].pos.y},
                {enemies[i].pos.x - 1, enemies[i].pos.y + 1},
                {enemies[i].pos.x + 1, enemies[i].pos.y + 1},
                {enemies[i].pos.x - 2, enemies[i].pos.y + 2},
                {enemies[i].pos.x + 2, enemies[i].pos.y + 2}
            };

            if (check_collision_enemies(ship_parts, enemy_parts, 5)) {// Comprueba colision entre jugador y enemigos
                enemies[i].active = 0;
                hp--;// Resta la vida por colision
                break;
            }
        }
    }
}

// Verifica si dos posiciones coinciden
int check_collision(Position pos, Position *parts, int size) {
    for (int i = 0; i < size; i++) {
        if (pos.x == parts[i].x && pos.y == parts[i].y) {
            return 1;
        }
    }
    return 0;
}

int check_collision_enemies(Position *ship_parts, Position *parts, int size) {
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (ship_parts[j].x == parts[i].x && ship_parts[j].y == parts[i].y) {
                return 1;
            }
        }
    }
    return 0;
}

// Dibuja los bordes de la pantalla
void draw_borders() {
    for (int i = 0; i < COLS; i++) {
        mvprintw(0, i, "#");
        mvprintw(2, i, "-");
        mvprintw(LINES - 1, i, "#");
    }
    for (int i = 0; i < LINES; i++) {
        mvprintw(i, 0, "#");
        mvprintw(i, COLS - 1, "#");
    }
}

// Dibuja la nave del jugador en su posición
void draw_ship(int x, int y) {
    
    
    // Dibujar la nave con colores
    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(    y, x - 4, "    A");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    attron(COLOR_PAIR(2)); // Aplicar color magenta
    mvprintw(y + 1, x - 4, "   MTM");
    mvprintw(y + 2, x - 4, "  WTTTW");
    attroff(COLOR_PAIR(2)); // Desactivar color magenta

    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(y + 3, x - 4, "TTTTHTTTT");
    mvprintw(y + 4, x - 4, "  UUUUU");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    // Actualizar la pantalla para mostrar cambios
    refresh();
    
    // Esperar entrada del usuario para salir
    getch();
    
    // Finalizar ncurses
    endwin();
}

// Dibuja un enemigo según su tipo
void draw_enemy(int x, int y, int type) {
    switch (type) {
        case 0:
            attron(COLOR_PAIR(3));
            mvprintw(y, x - 2, " (@@) ");
            mvprintw(y + 1, x - 2, " /\"\"\\ ");
            attroff(COLOR_PAIR(3));
            break;
        case 1:
            attron(COLOR_PAIR(5));
            mvprintw(y, x - 2, " dOOb ");
            mvprintw(y + 1, x - 2, " ^/\\^ ");
            attroff(COLOR_PAIR(5));
            break;
        case 2:
            attron(COLOR_PAIR(4));
            mvprintw(y, x - 2, " /MM\\ ");
            mvprintw(y + 1, x - 2, " |~~| ");
            attroff(COLOR_PAIR(4));
            break;
    }
}

// Actualiza la puntuación basada en el tipo de enemigo derrotado
void update_score(int type) {
    switch (type) {
        case 0:
            score += 30;
            break;
        case 1:
            score += 20;
            break;
        case 2:
            score += 10;
            break;
    }
}

// Muestra la pantalla de inicio con instrucciones para comenzar o salir
void draw_start_screen() {
    clear();
    attron(COLOR_PAIR(2));
    mvprintw(LINES / 2 - 2, COLS / 2 - 10, "Space Shooter Game");
    attroff(COLOR_PAIR(2));
    mvprintw(LINES / 2, COLS / 2 - 10, "Press 'n' to Start New Game");
    mvprintw(LINES / 2 + 1, COLS / 2 - 10, "Press 'q' to Quit");
    mvprintw(LINES / 2 + 2, COLS / 2 - 10, "High Score: %d", high_score);
    refresh();
}

// Muestra la pantalla de fin del juego con puntuación y opciones
void draw_game_over_screen() {
    clear();
    attron(COLOR_PAIR(4));
    mvprintw(LINES / 2 - 2, COLS / 2 - 10, "Game Over");
    attroff(COLOR_PAIR(4));
    mvprintw(LINES / 2, COLS / 2 - 10, "Press 'r' to Return to Start Screen");
    mvprintw(LINES / 2 + 1, COLS / 2 - 10, "Press 'q' to Quit");
    mvprintw(LINES / 2 + 2, COLS / 2 - 10, "Score: %d", score);
    mvprintw(LINES / 2 + 3, COLS / 2 - 10, "High Score: %d", high_score);
    refresh();
}
