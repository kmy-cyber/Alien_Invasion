# Documentación del Juego

## Introducción

Este juego, **MATCOM_INVASION**, está implementado en C utilizando la librería **ncurses**. El proyecto integra diversos conceptos relacionados con **Sistemas Operativos**, tales como la gestión de procesos, hilos, memoria y archivos. El jugador controla la nave para defender honorablemente su bella Facultad, navegando a través de una serie de desafíos con controles simples desde el teclado.

### Controles del Juego

- **Teclas de dirección**: Mover al jugador.
- **Barra espaciadora**: Disparar.
- **P**: Pausar el juego.
- **S**: Salvar el juego en el momento actual.
- **P**: Cargar algun juego salvado.
- **Q**: Salir del juego.

### Características del Juego

- **Modo de un jugador** con dificultad creciente. Las naves van incrementando velocidad y aparece un Boss cada cierto tiempo que te dispara.
- Opción de **Pausar y Reanudar** el juego.
- **Guardar/Cargar el Juego**: El estado actual del juego se puede guardar en un archivo y cargarse posteriormente.
- **Tabla de puntuaciones**: Rastrea los puntajes más altos.

---

## Estructura del Código

### Componentes Principales

El código está organizado en varios componentes clave:

1. **Inicialización del Juego**: 
   - Inicializa la pantalla con ncurses, configura los colores y prepara el entorno del juego.
   
2. **Bucle Principal del Juego**: 
   - Se ejecuta continuamente, manejando la entrada del usuario, actualizando el estado del juego y renderizando la salida.
   
3. **Manejo de Entradas**: 
   - Captura las entradas del usuario a través del teclado para controlar las acciones del jugador.
   
4. **Detección de Colisiones y Puntuación**: 
   - Detecta cuando el jugador colisiona con objetos o enemigos y ajusta la puntuación.

---

## Conceptos de Sistemas Operativos

### 1. **Hilos**

Se utilizan hilos para manejar diferentes aspectos del juego en paralelo, asegurando un juego fluido y la capacidad de respuesta a las entradas del usuario.

- **Hilo 1: Renderizado del Juego**  
  Este hilo maneja el renderizado de los elementos del juego, como el jugador, los enemigos y los proyectiles en la pantalla.

- **Hilo 2: Manejo de Entradas**  
  Este hilo escucha continuamente las entradas del usuario y actualiza el estado del juego en consecuencia (por ejemplo, movimiento, disparos).

- **Hilo 3: Lógica del Juego**  
  Este hilo gestiona la lógica del juego, como la detección de colisiones, la actualización de posiciones y la puntuación.

Los hilos fueron creados utilizando la librería `pthread`, lo que permite separar las tareas y garantizar que el juego se ejecute sin interrupciones, incluso cuando se realizan cálculos complejos.

### 2. **Gestión de Memoria**

El juego utiliza asignación dinámica de memoria para varios componentes como:

- Entidades del juego (jugador, enemigos, proyectiles).
- Estados del juego (guardar/cargar el juego).

Es permittido salvar hasta 3 estados del juego y luego cargar alguno de ellos para continuar jugando, si ya estan llenas las 3 casillas, el programa realiza un criterio de seleccion para colocar la nueva partida salvada. Usa el criterio de LRU(Last Recently used). 

### 3. **Planificador (Scheduler)**

El squeduler de memoria LRU

### 4. **Gestión de Archivos**

El juego soporta guardar y cargar estados del juego desde un archivo. Esta característica se implementa utilizando funciones estándar de entrada/salida de archivos en C:

- **Guardar el juego**: Almacena la posición actual del jugador, enemigos y puntuación en un archivo.
- **Cargar el juego**: Restaura el estado del juego desde un archivo, permitiendo al jugador continuar desde donde lo dejó.

### 5. **Gestión de Procesos**

Además de los hilos, el juego puede crear nuevos procesos para manejar ciertas tareas de larga duración, como guardar puntuaciones altas o realizar cálculos en segundo plano. La llamada al sistema `fork()` se utiliza para crear un nuevo proceso que opera independientemente del bucle principal del juego.

---

