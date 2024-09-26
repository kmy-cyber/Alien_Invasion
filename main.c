#pragma region _DEFINICIONES_Y_MACROS
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define DELAY 30000          // Tiempo de espera entre actualizaciones en microsegundos
#define MAX_PROJECTILES 5    // Máximo número de proyectiles que puede tener el jugador
#define BMAX_PROJECTILES 1   // Máximo número de proyectiles que puede tener el jefe
#define MAX_ENEMIES 10       // Máximo número de enemigos en el juego
#define SPEED_LOW_ENEMIES 10 // Velocidad de movimiento de los enemigos
#define MAX_SAVED_GAMES 3    // Maximo de partidas a guardar
#define min(x, y) x < y ? x : y
#define BOSS_TIME 5

typedef struct
{
    int x, y;
} Position;

typedef struct
{
    Position pos;
    int is_active; // Indica si el proyectil está activo o no
} Projectile;

typedef struct
{
    int high_score;
    int score;
    Position Ship;
    int lru;
    int health_points;
} Saved_Games;

typedef struct
{
    Position pos;
    int is_active; // Indica si el enemigo está activo o no
    int type;   // Tipo de enemigo (para variedad visual)
} Enemy;

typedef struct
{
    Position pos;
    int hp;
    int direction;
    int is_active;
    int is_arriving;
} Boss;

// void asd(){
//     struct Boss asd;
//     asd.
// }
#pragma endregion

#pragma region VARIABLES_GLOBALES
// Variables globales
Position player;                               // Posición del jugador
Projectile projectiles[MAX_PROJECTILES];       // Array de proyectiles del jugador
Projectile boss_projectiles[BMAX_PROJECTILES]; // Array de proyectiles del jefe
Enemy enemies[MAX_ENEMIES];                    // Array de enemigos
Boss boss;
Saved_Games saved_games[MAX_SAVED_GAMES];
Saved_Games loaded_game;
clock_t start_time;  // Tiempo inicial
double elapsed_time; // Tiempo transcurrido
int schedule_fifo_enemy[MAX_ENEMIES];

int running = 1;    // Variable para controlar el estado de ejecución del juego
int score = 0;      // Puntuación del jugador
int hp = 3;         // Vida del jugador
int high_score = 0; // Mejor puntuación alcanzada
int state = 0;      // Estado del juego (0: inicio, 1: jugando, 2: fin del juego)
int current_game = -1;
int enemy_died = 0;
pthread_mutex_t mutex; // Mutex para sincronizar acceso a recursos compartidos

#pragma endregion 

#pragma region DECLARACIONES_DE_FUNCIONES_PRINCIPALES
// Funciones del juego
void init_game();                // Inicializa el juego al iniciar una nueva partida
void *game_loop(void *arg);      // Bucle principal del juego
void *input_handler(void *arg);  // Manejador de entrada para capturar eventos del usuario
void move_player(int direction); // Mueve al jugador según la dirección indicada
void shoot();                    // Dispara un proyectil desde la posición del jugador
void update_projectiles();       // Actualiza la posición de los proyectiles
void update_boss_projectiles();
void update_enemies(); // Actualiza la posición de los enemigos
void spawn_boss();
void update_boss();                                                            // Actualiza la posición del jefe
void check_collisions();                                                       // Verifica colisiones entre proyectiles y enemigos
void draw_borders();                                                           // Dibuja los bordes de la pantalla
void draw_ship(int x, int y);                                                  // Dibuja el barco del jugador
void draw_enemy(int x, int y, int type);                                       // Dibuja un enemigo en la pantalla
void draw_boss(int x, int y);                                                  // Dibuja el Jefe
int check_collision(Position pos, Position *parts, int size);                  // Comprueba si hay colisión entre dos objetos
int check_collision_enemies(Position *ship_parts, Position *parts, int size);  // Comprueba si hay colisión entre el barco y los enemigos
int check_collision_boss(Position boss_projectile, Position *parts, int size); // Comprueba si hay colisión entre el proyectil del jefe y la nave
void update_score(int type);                                                   // Actualiza la puntuación basada en el tipo de enemigo derrotado
void draw_start_screen();                                                      // Dibuja la pantalla de inicio del juego
void draw_game_over_screen();                                                  // Dibuja la pantalla de fin del juego

void save_game(const char *filename, Saved_Games *game);          // Guarda partida en el estdo actual
int load_games(const char *filename, Saved_Games *game, int max); // Carga partidas guardadas
void display_games(Saved_Games saved_games[], int num_games);     // Muestra las partidas guardadas

#pragma endregion

#pragma region FUNCION_PRINCIPAL
// Función principal del programa
int main()
{
    srand(time(NULL)); // Inicializa la semilla para generar números aleatorios
    initscr();         // Inicia el modo ncurses
    noecho();          // Desactiva el eco de teclado
    curs_set(FALSE);   // Oculta el cursor
    timeout(0);        // Configura getch para ser no bloqueante
    start_color();     // iniciar color

    // Definir pares de colores
    init_pair(1, COLOR_BLUE, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);

    pthread_t game_thread, input_thread; // Declara los identificadores de los hilos para el juego y el manejo de entrada
    pthread_mutex_init(&mutex, NULL);    // Inicializa el mutex para sincronización

    // Crea los hilos para el bucle del juego y el manejo de entrada
    pthread_create(&game_thread, NULL, game_loop, NULL);
    pthread_create(&input_thread, NULL, input_handler, NULL);

    // Espera a que ambos hilos terminen antes de continuar
    pthread_join(game_thread, NULL);
    pthread_join(input_thread, NULL);

    endwin();                      // Finaliza el modo ncurses
    pthread_mutex_destroy(&mutex); // Destruye el mutex

    return 0;
}

// Inicializa el juego, reseteando variables y posicionando al jugador y elementos en sus estados iniciales
void init_game()
{
    player.x = COLS / 2;  // Coloca al jugador en el centro horizontalmente
    player.y = LINES - 9; // Coloca al jugador cerca del borde inferior
    score = 0;            // Resetea la puntuación
    hp = 3;               // Resetea la vida del jugador

    // Inicializa el arreglo de scheduler para la generacion de enemigos
    enemy_died = MAX_ENEMIES;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        schedule_fifo_enemy[i] = i + 1;
    }

    // Desactiva todos los proyectiles y enemigos al inicio de una nueva partida
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        projectiles[i].is_active = 0;
    }
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].is_active = 0;
    }
    for (int i = 0; i < BMAX_PROJECTILES; i++)
    {
        boss_projectiles[i].is_active = 0;
    }
    boss.is_active = 0;
}

// cargar valores del juego cargado
void startload_game(Saved_Games saved)
{
    player.x = saved.Ship.x;       // Coloca al jugador en el centro horizontalmente
    player.y = LINES - 9;          // Coloca al jugador cerca del borde inferior
    score = saved.score;           // Indica la puntuación al valor cargado
    hp = saved.health_points;      // Indica la vida del jugador al valor cargado
    high_score = saved.high_score; // Indica la mejor puntuacion al valor cargado

    // Inicializa el arreglo de scheduler para la generacion de enemigos
    enemy_died = MAX_ENEMIES;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        schedule_fifo_enemy[i] = i + 1;
    }

    // Desactiva todos los proyectiles y enemigos al inicio de una nueva partida
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        projectiles[i].is_active = 0;
    }
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].is_active = 0;
    }
    for (int i = 0; i < 5; i++)
    {
        boss_projectiles[i].is_active = 0;
    }
}

#pragma endregion

#pragma region BUCLE_PRINCIPAL
// Bucle principal del juego, controla el estado del juego y actualiza la pantalla según el estado actual
void *game_loop(void *arg)
{
    int move_enemy = 0; // controla el mover de los enemigos
    int clock_flag = 0; // controla el inicio del reloj

    // Main loop
    while (running)
    {
        pthread_mutex_lock(&mutex); // Bloquea mutex para acceso seeguro a las variables
        // Dependiendo del estado, muestra la pantalla de inicio, actualiza el juego o la pantalla de fin de juego
        if (state == 0)
        {
            draw_start_screen();
        }
        else if (state == 1)
        {

            update_projectiles();      // Actualiza proyectiles
            update_boss_projectiles(); // Actualiza los proyectiles del jefe

            // Actualiza enemigos si es necesario
            if (!move_enemy)
            {
                update_enemies();
            }

            check_collisions(); // verifica las colisiones

            // Verificar si hay que actualizar la puntuacion mas alta
            if (hp <= 0)
            {
                state = 2;
                if (score > high_score)
                {
                    high_score = score;
                }
            }
            if (score > high_score)
            {
                high_score = score;
            }

            clear();        // Limpia la pantalla antes de dibujar
            draw_borders(); // Dibuja los bordes

            if (boss.is_active)
            {
                update_boss();
                draw_boss(boss.pos.y, boss.pos.x);
            }

            if (!clock_flag)
            { // Echamos a andar el timer
                clock_flag = 1;
                start_time = clock();
            }
            elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC; // Calcular el tiempo transcurrido

            if (elapsed_time >= BOSS_TIME && !boss.is_active)
            {
                spawn_boss();
            }

            draw_ship(player.x, player.y); // Dibuja al jugador

            // Imprimir
            mvprintw(1, 2, "HP: %d", hp);
            mvprintw(1, 10, "Score: %d", score);
            mvprintw(1, 20, "High Score: %d", high_score);
            if (boss.is_active)
            {
                mvprintw(1, 40, "Boss HP %d", boss.hp);
            }
            for (int i = 0; i < MAX_PROJECTILES; i++)
            {
                if (projectiles[i].is_active)
                {
                    mvprintw(projectiles[i].pos.y, projectiles[i].pos.x, "|");
                }
            }
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (enemies[i].is_active)
                {
                    draw_enemy(enemies[i].pos.x, enemies[i].pos.y, enemies[i].type);
                }
            }
            for (int i = 0; i < BMAX_PROJECTILES; i++)
            {
                if (boss_projectiles[i].is_active)
                {
                    mvprintw(boss_projectiles[i].pos.x, boss_projectiles[i].pos.y, "U");
                }
            }
            refresh(); // Actualiza la pantalla con los cambios
        }
        else if (state == 2)
        {
            draw_game_over_screen();
        }

        pthread_mutex_unlock(&mutex); // Desbloquea el mutex

        usleep(DELAY);                                     // espera el proximo ciclo
        move_enemy = (move_enemy + 1) % SPEED_LOW_ENEMIES; // Controla la velocidad de los enemigos
    }
    return NULL;
}

#pragma endregion

#pragma region MANEJO_ENTRADA
// Maneja la entrada del usuario para controlar el juego
void *input_handler(void *arg)
{
    int ch;
    while (running)
    {
        ch = getch();               // obtiene la entrada del usuario
        pthread_mutex_lock(&mutex); // Bloquea el mutex

        // Cambia el estado del juego segun la entrada
        if (state == 0)
        {
            if (ch == 'n')
            {
                state = 1;
                current_game = -1;
                init_game();
            }
            else if (ch == 'q')
            {
                running = 0;
            }
            else if (ch == 'l')
            {
                state = 1;
                // Cargar todos los juegos desde el archivo
                int num_games = load_games("saved_games.dat", saved_games, MAX_SAVED_GAMES);

                // Mostrar los juegos cargados
                display_games(saved_games, num_games);

                // Esperar a que el usuario ingrese un número para seleccionar un juego
                int choice;
                do
                {
                    mvprintw(LINES - 2, 2, "Select a game to load (1 to %d): ", num_games);
                    choice = getch();

                    if (choice == 'q')
                    {
                        state = 0;
                        break;
                    }

                    choice -= '0';

                    if (choice < 1 || choice > num_games)
                    {
                        mvprintw(LINES - 1, 2, "Invalid selection. Please try again.");
                        refresh();
                        usleep(DELAY); // Espera un poco antes de volver a pedir la entrada
                    }
                } while (choice < 1 || choice > num_games);

                if (state == 0)
                {
                    continue;
                }

                // Guardar que juego se va a cargar
                current_game = choice - 1;

                for (int i = 0; i < MAX_SAVED_GAMES; i++)
                {
                    if (saved_games[i].lru > saved_games[current_game].lru)
                    {
                        saved_games[i].lru--;
                    }
                }

                // Cargar el juego seleccionado
                saved_games[current_game].lru = num_games;
                loaded_game = saved_games[current_game];

                // Sobreescribir las partidas guardadas del juego en un archivo
                save_game("saved_games.dat", saved_games);

                startload_game(loaded_game);
                state = 1; // Regresar al estado de juego después de cargar un juego
            }
        }
        else if (state == 1)
        {
            switch (ch)
            {
            case 'a':
            case KEY_LEFT:
                move_player(KEY_LEFT);
                break;
            case 'd':
            case KEY_RIGHT:
                move_player(KEY_RIGHT);
                break;
            case ' ':
                shoot();
                break;
            case 'q':
                state = 0;
                break;
            case 's':
                // Cargar todos los juegos desde el archivo
                int num_games = load_games("saved_games.dat", saved_games, 3);
                num_games = min(num_games, MAX_SAVED_GAMES - 1);

                Saved_Games game = {high_score,
                                    score,
                                    {player.x, player.y},
                                    num_games + 1,
                                    hp};

                if (current_game == -1)
                {
                    int saved_game_successfull = 0, old_game_index = -1;

                    for (int i = 0; i < MAX_SAVED_GAMES; i++)
                    {
                        if (saved_games[i].lru == 0)
                        {
                            saved_games[i] = game;
                            saved_game_successfull = 1;
                            break;
                        }
                        if (saved_games[i].lru == 1)
                        {
                            old_game_index = i;
                        }
                    }

                    if (!saved_game_successfull)
                    {
                        for (int i = 0; i < MAX_SAVED_GAMES; i++)
                        {
                            saved_games[i].lru--;
                        }

                        saved_games[old_game_index] = game;
                    }
                }
                else
                {
                    for (int i = 0; i < MAX_SAVED_GAMES; i++)
                    {
                        if (saved_games[i].lru > saved_games[current_game].lru)
                        {
                            saved_games[i].lru--;
                        }
                    }

                    saved_games[current_game] = game;
                }

                // Guardar el juego en un archivo
                save_game("saved_games.dat", saved_games);
                break;
            }
        }
        else if (state == 2)
        {
            if (ch == 'r')
            {
                state = 0;
            }
            else if (ch == 'q')
            {
                running = 0;
            }
        }

        pthread_mutex_unlock(&mutex); // Desblqouea el mutex
    }
    return NULL;
}

#pragma endregion

#pragma region FUNCIONES_DE_ACTUALIZACION
// Mueve al jugador según la dirección ingresada por el usuario
void move_player(int direction)
{
    if (direction == KEY_LEFT && player.x > 2)
    {
        player.x--;
    }
    else if (direction == KEY_RIGHT && player.x < COLS - 3)
    {
        player.x++;
    }
}

// Dispara un proyectil desde la posicion del jugador
void shoot()
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (!projectiles[i].is_active)
        {
            projectiles[i].pos.x = player.x;
            projectiles[i].pos.y = player.y - 1;
            projectiles[i].is_active = 1;
            break;
        }
    }
}

// Actualiza las posiciones de los proyectiles activos
void update_projectiles()
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (projectiles[i].is_active)
        { // Mueve el proyectil hacia arriba y lo desactiva si sale de pantalla
            projectiles[i].pos.y--;
            if (projectiles[i].pos.y < 3)
            {
                projectiles[i].is_active = 0;
            }
        }
    }
}

// Actualiza las posiciones y estados de los enemigos
void update_enemies()
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].is_active)
        {
            enemies[i].pos.y++;
            if (enemies[i].pos.y >= LINES - 3)
            {
                enemies[i].is_active = 0;
                schedule_fifo_enemy[i] = enemy_died + 1;
                enemy_died++;
            }
        }
    }
    
    int lenght_cicle = enemy_died;

    mvprintw(LINES - 2, 2, "(%d): ", enemy_died);
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        mvprintw(LINES - 2, 2 * (i + 1), "%d ", schedule_fifo_enemy[i]);
    }
    

    for (int i = 0; i < lenght_cicle; i++)
    {
        if (enemy_died >= 1 && rand() % 100 < 5)
        {
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if(schedule_fifo_enemy[i] == 1)
                {
                    enemies[i].pos.x = 2 + rand() % (COLS - 4);
                    enemies[i].pos.y = 3;
                    enemies[i].is_active = 1;
                    enemies[i].type = rand() % 3; // 3 tipos de enemigos
                }

                if(schedule_fifo_enemy[i] >= 1)
                {
                    schedule_fifo_enemy[i]--;
                }
            }

            enemy_died--;
        }
    }
}

/* Boss Section*/
void spawn_boss()
{
    boss.is_active = 1;
    boss.is_arriving = 1;
    boss.direction = rand() % 2;
    boss.hp = 5;
    boss.pos.x = 6;
    boss.pos.y = COLS / 2;
    start_time = clock();
    draw_boss(boss.pos.x, boss.pos.y);
}

void draw_boss(int x, int y)
{
    // Disenno del boss
    /*
     /\^/\
    ( o o )
     \ v /
     /-"-\
    */
    // Dibujar la nave con colores

    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(y - 3, x, "/\\^/\\");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    attron(COLOR_PAIR(2)); // Aplicar color magenta
    mvprintw(y - 2, x - 1, "( o o )");
    mvprintw(y - 1, x, "\\ v /");
    attroff(COLOR_PAIR(2)); // Desactivar color magenta

    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(y, x, "/-\"-\\ ");
    // mvprintw(y, x, "A");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    // // Actualizar la pantalla para mostrar cambios
    // refresh();

    // // Esperar entrada del usuario para salir
    // getch();

    // // Finalizar ncurses
    // endwin();
}

void update_boss()
{
    if (boss.is_active)
    {
        if(boss.is_arriving)
        {
            if(boss.pos.x == LINES/2 - 4)
            {
                boss.is_arriving = 0;
            }
            boss.pos.x++;
        }
        else
        {
            if (boss.pos.y == 6 || boss.pos.y == COLS - 10)
            {
                boss.direction = (boss.direction + 1) % 2;
            }
            boss.pos.y = boss.direction == 0 ? boss.pos.y - 1 : boss.pos.y + 1;
        }
    }
}

void update_boss_projectiles()
{
    for (int i = 0; i < BMAX_PROJECTILES; i++)
    {
        if (boss_projectiles[i].is_active)
        {
            if (boss_projectiles[i].pos.x == LINES - 2)
            {
                boss_projectiles[i].is_active = 0;
                continue;
            }
            boss_projectiles[i].pos.x += 1;
        }
        else
        {
            if (boss.is_active && !boss.is_arriving)
            {
                boss_projectiles[i].is_active = 1;
                boss_projectiles[i].pos.x = boss.pos.x + 1;
                boss_projectiles[i].pos.y = boss.pos.y + 2;
            }
        }
    }
}

#pragma endregion

#pragma region FUNCIONES_CHECK_COLISIONES
// Verifica colisiones entre proyectiles y enemigos, y entre el jugador y enemigos
void check_collisions()
{
    Position ship_parts[] = {// Define las partes del jugador para colisiones
                             {player.x, player.y},
                             {player.x - 1, player.y + 1},
                             {player.x - 1, player.y - 1},
                             {player.x - 2, player.y - 2},
                             {player.x - 2, player.y + 2}};

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (projectiles[i].is_active)
        {
            // Comprueba cada proyectil del jugador contra todos los enemigos incluyendo al jefe
            for (int j = 0; j < MAX_ENEMIES; j++)
            {
                if (enemies[j].is_active)
                {
                    Position enemy_parts[] = {
                        {enemies[j].pos.x, enemies[j].pos.y},
                        {enemies[j].pos.x - 1, enemies[j].pos.y + 1},
                        {enemies[j].pos.x + 1, enemies[j].pos.y + 1},
                        {enemies[j].pos.x - 2, enemies[j].pos.y + 2},
                        {enemies[j].pos.x + 2, enemies[j].pos.y + 2}};

                    if (check_collision(projectiles[i].pos, enemy_parts, 5))
                    {
                        // Comprueba colision entre jugador y enemigos
                        projectiles[i].is_active = 0;
                        update_score(enemies[j].type); // Suma puntos por tipo de enemigo derrotado
                        enemies[j].is_active = 0;         // Desactiva el enemigo golpeado
                        break;
                    }
                }
            }

            if (boss.is_active)
            {
                Position boss_parts[] = {
                    {boss.pos.y + 5, boss.pos.x},
                    {boss.pos.y + 4, boss.pos.x},
                    {boss.pos.y + 3, boss.pos.x},
                    {boss.pos.y + 2, boss.pos.x},
                    {boss.pos.y + 1, boss.pos.x},
                    {boss.pos.y, boss.pos.x},
                };

                if (check_collision(projectiles[i].pos, boss_parts, 6))
                {
                    boss.hp--;
                    projectiles[i].is_active = 0;
                    if (boss.hp == 0)
                    {
                        update_score(3);
                        boss.is_active = 0;
                        start_time = clock();
                    }
                }
            }
        }
    }

    for (int i = 0; i < BMAX_PROJECTILES; i++)
    {
        if (check_collision_boss(boss_projectiles[i].pos, ship_parts, 5))
        {
            boss_projectiles[i].is_active = 0;
            hp--;
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].is_active)
        {
            Position enemy_parts[] = {
                {enemies[i].pos.x, enemies[i].pos.y},
                {enemies[i].pos.x - 1, enemies[i].pos.y + 1},
                {enemies[i].pos.x + 1, enemies[i].pos.y + 1},
                {enemies[i].pos.x - 2, enemies[i].pos.y + 2},
                {enemies[i].pos.x + 2, enemies[i].pos.y + 2}};

            if (check_collision_enemies(ship_parts, enemy_parts, 5))
            { // Comprueba colision entre jugador y enemigos
                enemies[i].is_active = 0;
                schedule_fifo_enemy[i] = enemy_died + 1;
                enemy_died++;

                hp--; // Resta la vida por colision
                break;
            }
        }
    }
}

// Verifica si dos posiciones coinciden
int check_collision(Position pos, Position *parts, int size)
{
    for (int i = 0; i < size; i++)
    {
        // exit(0);
        if (pos.x == parts[i].x && pos.y == parts[i].y)
        {
            return 1;
        }
    }
    return 0;
}

int check_collision_enemies(Position *ship_parts, Position *parts, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            if (ship_parts[j].x == parts[i].x && ship_parts[j].y == parts[i].y)
            {
                return 1;
            }
        }
    }
    return 0;
}

int check_collision_boss(Position boss_projectile, Position *ship_parts, int size)
{
    for (int i = 0; i < size; i++)
    {
        if(boss_projectile.x == ship_parts[i].y && boss_projectile.y == ship_parts[i].x)
        {
            return 1;
        }
    }

    return 0;
}

#pragma endregion

#pragma region FUNCIONES_DE_DIBUJO
// Dibuja los bordes de la pantalla
void draw_borders()
{
    for (int i = 0; i < COLS; i++)
    {
        mvprintw(0, i, "#");
        mvprintw(2, i, "-");
        mvprintw(LINES - 1, i, "#");
    }
    for (int i = 0; i < LINES; i++)
    {
        mvprintw(i, 0, "#");
        mvprintw(i, COLS - 1, "#");
    }
}

void draw_ship(int x, int y)
{

    // Dibujar la nave con colores
    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(y, x, "A");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    attron(COLOR_PAIR(2)); // Aplicar color magenta
    mvprintw(y + 1, x - 1, "MTM");
    mvprintw(y + 2, x - 2, "WTTTW");
    attroff(COLOR_PAIR(2)); // Desactivar color magenta

    attron(COLOR_PAIR(1)); // Aplicar color azul
    mvprintw(y + 3, x - 4, "TTTTHTTTT");
    mvprintw(y + 4, x - 2, "UUUUU");
    attroff(COLOR_PAIR(1)); // Desactivar color azul

    // Actualizar la pantalla para mostrar cambios
    refresh();

    // Esperar entrada del usuario para salir
    getch();

    // Finalizar ncurses
    endwin();
}

// Dibuja un enemigo según su tipo
void draw_enemy(int x, int y, int type)
{
    switch (type)
    {
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

#pragma endregion

#pragma region FUNCIONES_PUNTUACION_INICIO_FIN
// Actualiza la puntuación basada en el tipo de enemigo derrotado
void update_score(int type)
{
    switch (type)
    {
    case 0:
        score += 30;
        break;
    case 1:
        score += 20;
        break;
    case 2:
        score += 10;
        break;
    case 3:
        score += 50;
        break;
    }
}

// Muestra la pantalla de inicio con instrucciones para comenzar o salir
void draw_start_screen()
{
    clear();
    attron(COLOR_PAIR(1));
    mvprintw(LINES / 2 - 2, COLS / 2 - 10, "Space Shooter Game");
    attroff(COLOR_PAIR(1));
    mvprintw(LINES / 2, COLS / 2 - 10, "Press 'n' to Start New Game");
    mvprintw(LINES / 2 + 1, COLS / 2 - 10, "Press 'q' to Quit");
    mvprintw(LINES / 2 + 2, COLS / 2 - 10, "High Score: %d", high_score);
    mvprintw(LINES / 2 + 3, COLS / 2 - 10, "Press 'l' to Load Games");
    refresh();
}

// Muestra la pantalla de fin del juego con puntuación y opciones
void draw_game_over_screen()
{
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

#pragma endregion

#pragma region FUNCIONES_GUARDADO_Y_CARGA
void save_game(const char *filename, Saved_Games *saved_games)
{
    FILE *file = fopen(filename, "wb"); // Abre el archivo en modo append binario
    if (file == NULL)
    {
        perror("Error opening file for writing");
        return;
    }

    size_t written = fwrite(saved_games, sizeof(Saved_Games), MAX_SAVED_GAMES, file);
    if (written != 1)
    {
        perror("Error writing to file");
    }

    fclose(file);
}

int load_games(const char *filename, Saved_Games saved_games[MAX_SAVED_GAMES], int max_games)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("Error opening file for reading");
        return 0;
    }

    size_t read = fread(saved_games, sizeof(Saved_Games), max_games, file);
    if (ferror(file))
    {
        perror("Error reading from file");
    }

    fclose(file);

    int cant_games = 0;
    for (int i = 0; i < max_games; i++)
    {
        cant_games += saved_games[i].lru > 0;
    }

    return cant_games;
}

void display_games(Saved_Games saved_games[], int num_games)
{
    clear();
    int row = 2; // Iniciar en la fila 2 para dejar espacio para el encabezado
    for (int i = 0; i < num_games; i++)
    {
        mvprintw(row, 2, "Game %d:", i + 1);
        mvprintw(row + 1, 4, "High Score: %d", saved_games[i].high_score);
        mvprintw(row + 2, 4, "LRU: %d", saved_games[i].lru);
        mvprintw(row + 3, 4, "Health Points: %d", saved_games[i].health_points);
        mvprintw(row + 4, 4, "Score: %d", saved_games[i].score);
        row += 6; // Saltar 6 filas entre juegos para dejar espacio
    }
    refresh();
}
#pragma endregion