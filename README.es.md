# Shrinkigon

[English](README.md) | **Español**

Shrinkigon es una herramienta de escritorio para macOS que simplifica mallas 3D. Carga un modelo, reduce su cantidad de triángulos con un algoritmo de colapso de aristas basado en métricas de error cuádrico, muestra estadísticas de antes y después, previsualiza el resultado en un visor 3D interactivo y exporta el modelo optimizado. También incluye una extensión opcional de procesamiento en la nube llamada **Speedigon**.

Está escrita en C++17 con Dear ImGui, OpenGL 2.1, Assimp y OpenMesh, y está pensada para desarrolladores que quieran una herramienta de reducción de mallas pequeña y legible.

> ## Speedigon es una simulación, no una compra real
> El botón "Comprar ahora" y el precio de "$5 USD" que muestra la aplicación son **marcadores de posición**. No se procesa ningún pago ni hay ninguna pasarela de pago integrada. Al pulsar el botón solo se marca `speedigon = true` en el registro del usuario en la base de datos, para poder probar la función de punta a punta. No tomes este proyecto como un producto comercial ni como ejemplo de un flujo de pago.

## Tabla de contenido

- [Características](#características)
- [Requisitos](#requisitos)
- [Compilación](#compilación)
- [Configuración](#configuración)
- [Uso](#uso)
- [Speedigon y el backend en la nube](#speedigon-y-el-backend-en-la-nube)
- [Algoritmos](#algoritmos)
- [Limitaciones conocidas](#limitaciones-conocidas)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Notas de seguridad](#notas-de-seguridad)
- [Software de terceros](#software-de-terceros)
- [Licencia](#licencia)

## Características

- Carga modelos `.gltf`, `.glb`, `.obj` y `.fbx` con Assimp, incluidas sus texturas.
- Reducción por colapso de aristas con error cuádrico, con restricciones para bordes, aristas duras, inversión de normales y mapeo UV.
- Visor 3D interactivo (rotar, desplazar, acercar) sobre una placa base, parecida a la de los editores de motores de videojuegos.
- Estadísticas de antes y después: vértices, aristas y memoria.
- Un botón "Remesh del modelo" que devuelve el modelo a como estaba antes de la optimización.
- Exporta el modelo optimizado junto a la carpeta original.
- Interfaz en inglés, español, alemán e italiano (inglés por defecto).
- Cuentas opcionales y la extensión **Speedigon**, que ejecuta la optimización en un servidor de CPU en la nube y elimina el límite local de triángulos.
- Un tema neón que se aplica mientras Speedigon está activo.

## Requisitos

- macOS. El proyecto es solo para macOS: usa el framework OpenGL del sistema y `osascript` para el selector de carpetas. Compílalo en la arquitectura en la que lo vayas a ejecutar (Apple Silicon o Intel).
- Xcode Command Line Tools (`xcode-select --install`).
- CMake y un compilador de C++17.
- Bibliotecas: GLFW, FreeType, Assimp, OpenMesh y libcurl. Dear ImGui (con el backend de GLFW y el renderizador de OpenGL 2.1) y `stb_image.h` están incluidos en este repositorio.

Con Homebrew:

```bash
brew install cmake glfw freetype assimp open-mesh curl
```

## Compilación

```bash
git clone https://github.com/<tu-usuario>/shrinkigon.git
cd shrinkigon
cp cloud_config.example.h cloud_config.h
cmake -S . -B build
cmake --build build
```

La aplicación busca sus fuentes en una carpeta `fonts/` **relativa al directorio de trabajo**, así que ejecuta el programa desde la raíz del repositorio:

```bash
./build/<nombre-del-ejecutable>
```

## Configuración

Las cuentas y Speedigon necesitan un backend que despliegas tú mismo (ver [Speedigon y el backend en la nube](#speedigon-y-el-backend-en-la-nube)). Sus direcciones están en `cloud_config.h`, que a propósito no se sube al repositorio:

| Constante | Significado |
| --- | --- |
| `kCloudApiBaseUrl` | URL base de la API HTTP que maneja las cuentas (registro, inicio de sesión, perfil, idioma, compra). |
| `kSpeedigonApiBaseUrl` | URL base del servidor de optimización (`/optimize`). |
| `kCloudAuthTimeoutSeconds` | Tiempo máximo de las peticiones de cuenta. |
| `kCloudOptimizeTimeoutSeconds` | Tiempo máximo de una petición de optimización. |

Mientras una URL contenga `REPLACE_ME`, las funciones en la nube indican "no configurado" y todo lo demás sigue funcionando en local. Este repositorio no da acceso a ningún backend alojado.

Los ajustes del usuario (idioma, nombre de usuario y token de sesión) se guardan en `~/.shrinkigon_settings`.

## Uso

1. Pulsa **Subir recurso** y elige la carpeta que contiene el modelo. Shrinkigon usa el primer archivo `.gltf`, `.glb`, `.obj` o `.fbx` que encuentre en esa carpeta. El modelo aparece en el visor y el panel **Información del recurso** muestra sus vértices, aristas y memoria.
2. Ajusta los parámetros de optimización (tabla más abajo) y pulsa **Optimizar modelo**.
3. Al terminar, el panel muestra las cifras optimizadas y la reducción. Puedes inspeccionar el resultado en el visor.
4. Pulsa **Remesh del modelo** para volver al modelo como estaba antes de la optimización y probar otros parámetros.
5. Pulsa **Guardar modelo optimizado** para exportarlo.

**Controles del visor:** arrastra con el botón izquierdo para rotar, con el derecho o el central para desplazar, y usa la rueda del ratón para acercar o alejar.

### Parámetros

| Parámetro | Efecto |
| --- | --- |
| Porcentaje de reducción | Porcentaje de triángulos a eliminar (0 a 100). Es lo que define la meta del algoritmo. |
| Evitar inversión de normales | Rechaza los colapsos que giren más de unos 72,5 grados cualquier cara afectada. |
| Preservar aristas duras | No colapsa aristas cuyas dos caras forman un ángulo mayor que el umbral. |
| Ángulo de arista dura | Umbral de la opción anterior, en grados (por defecto 45). |
| Bloquear aristas de borde | Mantiene fijos los vértices de los bordes abiertos. |
| Preservar mapeo UV | Mantiene fijos los vértices de las costuras UV y rechaza los colapsos que deformen el mapeo de la textura. |
| Triángulos objetivo | Aparece en la interfaz pero **el algoritmo todavía no lo usa**. |
| Error máximo de deformación | Aparece en la interfaz pero **el algoritmo todavía no lo usa**. |

### Límites

- Sin Speedigon, los modelos de más de **100.000 triángulos** no se pueden optimizar. Una ventana lo explica y señala Speedigon.
- Una optimización se detiene a los **60 segundos**. El modelo queda intacto y un mensaje explica lo ocurrido.
- Con Speedigon activo no hay límite de triángulos, pero el servidor detiene cada trabajo a los 50 segundos.

### Guardado

El modelo optimizado se escribe en una carpeta llamada `<carpeta de entrada>_shrinkigonized`, junto a la carpeta de entrada. Si ya existe una carpeta con ese nombre, se **elimina y se vuelve a crear**. El modelo se exporta en su formato original cuando Assimp lo permite, y como `.obj` en caso contrario.

## Speedigon y el backend en la nube

Speedigon es una extensión opcional. Cuando está activa, la optimización se envía a un servidor que ejecuta el mismo algoritmo en una instancia de CPU, y se elimina el límite local de triángulos. Si no se puede llegar al servidor o falla, la aplicación optimiza en local y te avisa.

Para usarla, el usuario debe crear una cuenta, pulsar **Comprar ahora** (simulado, ver el aviso al inicio) y luego **Activar Speedigon** en la configuración.

### Arquitectura

```
Shrinkigon (C++, libcurl)
   |-- cuentas ---> API HTTP (API Gateway) --> Lambda (Node.js) --> DynamoDB
   |-- /optimize -> servidor de optimización (C++, cpp-httplib, EC2)
                        |-- valida el token con GET /me en la API HTTP
```

- **Cuentas.** Una tabla de DynamoDB con clave `username` guarda `passwordHash`, `language` y `speedigon`. Las contraseñas se almacenan con scrypt (sal aleatoria de 16 bytes). Las sesiones usan un token con la forma `base64url(payload).base64url(HMAC-SHA256)` que vence a los 30 días. Los nombres de usuario van en minúsculas, de 3 a 32 caracteres (`A-Z a-z 0-9 . _ -`); las contraseñas, de 8 a 128 caracteres.
- **Servidor de optimización.** `server/speedigon_server.cpp` es un pequeño servidor HTTP en el puerto 8080. Por cada petición comprueba el token, decodifica las mallas, ejecuta la reducción y devuelve el resultado. Procesa un trabajo a la vez y responde `busy` si hay otro en curso.

### API HTTP

| Método y ruta | Atendida por | Propósito |
| --- | --- | --- |
| `POST /register` | Lambda | Crear una cuenta. |
| `POST /login` | Lambda | Iniciar sesión y obtener un token. |
| `GET /me` | Lambda | Perfil y valor de `speedigon`. |
| `POST /language` | Lambda | Guardar la preferencia de idioma. |
| `POST /purchase` | Lambda | **Compra simulada:** marca `speedigon = true` para el usuario autenticado. |
| `POST /optimize` | Servidor EC2 | Optimizar mallas (requiere `speedigon = true`). La Lambda tiene un esqueleto de esta ruta que responde 501. |

Todas las respuestas son JSON con un campo `ok` y, si falla, un `code` como `unauthorized`, `invalid_credentials`, `username_taken`, `not_owned`, `busy` o `timeout`.

### Formato de intercambio de la optimización

El cuerpo de la petición es `{"config": {...}, "payload": "<base64>"}`. El payload es binario, little-endian: un `uint32` con la cantidad de mallas y, por cada malla, un `uint32` con la cantidad de vértices, un `uint32` con la cantidad de índices, `vértices x 8` valores `float32` (posición xyz, normal xyz, u, v) y los `índices` como `uint32`. La respuesta es `{"ok": true, "payload": "<base64>"}` con la misma estructura. Las texturas y materiales nunca salen del equipo: el cliente los conserva y solo reemplaza la geometría.

### Desplegar tu propio backend

1. **DynamoDB:** crea una tabla con clave de partición `username` (String).
2. **Lambda:** crea una función de Node.js con el código de `aws/lambda/index.mjs`. Define las variables de entorno `TABLE_NAME` y `TOKEN_SECRET` (una cadena aleatoria larga, por ejemplo `openssl rand -hex 32`). Dale a su rol los permisos `dynamodb:GetItem`, `dynamodb:PutItem` y `dynamodb:UpdateItem` sobre la tabla.
3. **API Gateway:** crea una API HTTP con las rutas `POST /register`, `POST /login`, `GET /me`, `POST /language` y `POST /purchase`, todas integradas con la Lambda. Si adjuntas las integraciones desde la CLI, concede también a API Gateway el permiso de invocar la función (`aws lambda add-permission`).
4. **Servidor de optimización:** en una instancia Linux instala `g++`, `libopenmesh-dev`, `libcpp-httplib-dev` y `libcurl4-openssl-dev`, y compila:

   ```bash
   g++ -O3 -std=c++17 -pthread server/speedigon_server.cpp mesh_codec.cpp mesh_chunking.cpp optimization_algorithms.cpp -I. -lOpenMeshCore -lcurl -o speedigon_server
   ```

   Ejecútalo con `SPEEDIGON_AUTH_URL` apuntando a la URL base de la API HTTP (hay una unidad de systemd de ejemplo en `server/speedigon.service`) y abre el puerto 8080 en el grupo de seguridad.
5. Pon ambas URL en `cloud_config.h` y recompila la aplicación.

## Algoritmos

Todo el procesamiento geométrico está en `optimization_algorithms.cpp`. Esta sección describe, paso a paso, lo que hace. El enfoque sigue el método de métricas de error cuádrico de Garland y Heckbert (1997), con restricciones adicionales para modelos con textura.

### 1. Importación

Los modelos se leen con Assimp con estas opciones de posprocesado: triangular, generar normales suaves, unir vértices idénticos, invertir UV y pretransformar vértices (se aplana la jerarquía de nodos y se aplican todas las transformaciones a los vértices, lo que normalmente deja una malla por material). Cada malla de Assimp se convierte en un `MeshData`: un arreglo de vértices (posición, normal, u, v), una lista de índices de triángulos y la información de su material (textura o color difuso). Las texturas se cargan con `stb_image`.

La reducción se ejecuta **de forma independiente en cada malla**; nunca se colapsan aristas entre dos mallas.

### 2. Conversión a una malla de semiaristas

Cada `MeshData` se convierte en una malla triangular de OpenMesh (`TriMesh_ArrayKernelT`) con coordenadas de textura en las semiaristas:

1. **Soldado de vértices.** Las posiciones se cuantizan con `epsilon = max(1e-6 x diagonal de la caja envolvente, 1e-9)` y los vértices que caen en la misma celda pasan a ser uno solo. Los modelos suelen guardar un vértice varias veces (una por cada discontinuidad de UV o de normal); el soldado recupera la conectividad real.
2. **Caras.** Se omite toda cara cuyas esquinas se reducen a menos de tres vértices distintos. Si OpenMesh rechaza una cara porque crearía una configuración no variedad ("complex vertex"), se duplican sus tres vértices y se vuelve a agregar.
3. **UV.** Las coordenadas de textura se guardan **por esquina de cara** (en las semiaristas), no por vértice. Eso permite que las costuras UV sobrevivan al soldado.

### 3. Tamaño objetivo

El porcentaje se limita al rango de 0 a 100 y la meta es

```
objetivo = max(1, floor(triángulosOriginales x (1 - porcentaje / 100)))
```

Si la meta no es menor que la cantidad original, la malla se devuelve sin cambios. El colapso termina en cuanto el número de triángulos alcanza la meta o cuando no queda ningún colapso válido, así que una malla muy restringida puede quedar por encima de la meta.

### 4. Métrica de error cuádrico

Para cada cara con normal unitaria `n`, área `A` y cualquiera de sus puntos `x0`, se define el plano `p = (a, b, c, d)` con `(a, b, c) = n` y `d = -n . x0`. Su cuádrica fundamental es la matriz de 4x4 `K = p p^T`, de modo que `v^T K v` es la distancia al cuadrado desde `v = (x, y, z, 1)` hasta el plano. La cuádrica de un vértice es la suma **ponderada por área** sobre sus caras incidentes:

```
Q(vértice) = suma sobre las caras incidentes de  A_cara x K_cara
```

Las caras degeneradas (con área prácticamente cero) se ignoran. El costo de colapsar la arista entre los vértices 1 y 2 en un punto `v` es

```
costo(v) = v^T (Q1 + Q2) v
```

### 5. Dónde queda el vértice resultante

Para cada arista se construye un conjunto de posiciones candidatas y se conserva la de menor costo:

- **Ambos extremos en un borde** (solo posible si los bordes no están bloqueados): la propia arista debe ser de borde; los candidatos son los dos extremos.
- **Un extremo en un borde:** el único candidato es la posición de ese extremo, para que el borde no se mueva.
- **Arista interior, sin preservar UV:** se agrega la posición *óptima* cuando existe y está a una distancia del punto medio no mayor que la longitud de la arista. Resuelve `A x = -b`, donde `A` es el bloque superior izquierdo de 3x3 de `Q1 + Q2` y `b` son las tres primeras entradas de su última columna. El sistema se resuelve con la regla de Cramer y se descarta si `|det A| < 1e-12`. Los dos extremos y el punto medio son siempre candidatos.
- **Arista interior, preservando UV:** no se usa la posición óptima (podría salirse de la arista y hacer que la interpolación de UV no tenga sentido). Los candidatos son los extremos, el punto medio y los puntos al 25 % y al 75 % de la arista.

El UV del vértice resultante es la interpolación lineal de los UV de los extremos, usando el parámetro `t` que resulta de proyectar la posición elegida sobre la arista (limitado entre 0 y 1).

### 6. Comprobaciones de validez

Un colapso candidato solo se acepta si se cumple todo lo siguiente:

1. **Topología.** El `is_collapse_ok` de OpenMesh pasa en al menos una dirección. Implementa la condición de enlace, que evita que el colapso cree geometría no variedad o pellizque la superficie.
2. **Vértices bloqueados.** Si *Bloquear aristas de borde* está activo, ningún extremo está en un borde y la arista no es de borde. Si *Preservar mapeo UV* está activo, ningún extremo pertenece a una costura UV. Una arista de costura es una arista interior cuyas dos caras discrepan en el UV de alguno de sus extremos en más de `sqrt(1e-10)`.
3. **Aristas duras.** Si *Preservar aristas duras* está activo, el ángulo entre las normales unitarias de las dos caras no debe superar el umbral.
4. **Inversión de normales.** Si *Evitar inversión de normales* está activo, se simula con la nueva posición cada cara que toque alguno de los extremos (salvo las que desaparecen). El colapso se rechaza si una cara nueva es degenerada o si `dot(n_vieja, n_nueva) < 0,3`, es decir, unos 72,5 grados.
5. **Distorsión de UV.** Si *Preservar mapeo UV* está activo, cada cara afectada se comprueba en el espacio UV: su área UV con signo debe conservar el signo (sin espejado ni pliegues), no debe degenerar y su *densidad UV* (área UV dividida entre el área 3D) debe cambiar por un factor entre 1/3 y 3.

### 7. El bucle de colapso

```
cuádricas  <- una cuádrica por vértice
versión[v] <- 0 para cada vértice
cola       <- montículo de mínimos ordenado por costo
para cada arista e: si e pasa las comprobaciones, insertar (costo, e, posición, uv, versión de ambos extremos)

mientras triángulos > objetivo y la cola no esté vacía:
    c <- extraer el candidato más barato
    descartar c si su arista o sus vértices fueron eliminados
    descartar c si versión[v1] o versión[v2] cambió desde que se encoló   (entrada obsoleta)
    elegir una dirección de colapso que is_collapse_ok acepte; si no, descartar
    eliminadas <- 2 caras en una arista interior, 1 en una de borde
    Q          <- Q(origen) + Q(destino)
    anillo     <- vecinos de ambos vértices
    colapsar la semiarista
    mover el vértice que sobrevive a c.posición y asignarle su UV
    Q(destino) <- Q
    triángulos -= eliminadas
    para cada vértice del anillo: versión++ y reevaluar cada arista incidente,
                                  insertando los nuevos candidatos
```

El montículo usa **invalidación perezosa**: las entradas nunca se quitan ni se editan; las obsoletas se descartan al extraerlas, lo cual se detecta con los contadores de versión por vértice. El tiempo es O(n log n) en la práctica y el proceso es secuencial.

### 8. Salida del resultado

La recolección de basura de OpenMesh elimina los elementos borrados y se recalculan las normales (normales de vértice de OpenMesh, es decir, la suma normalizada de las normales de las caras alrededor de cada vértice soldado). Después se reconstruye el arreglo de vértices de salida, separando de nuevo los vértices que tienen UV distintos, de modo que las costuras UV se restauran en la malla exportada. La información de textura y material se copia de la malla original.

### 9. Límite de tiempo y cancelación

La optimización corre en un hilo de trabajo para que la interfaz siga respondiendo. Una bandera atómica de cancelación se comprueba mientras se construyen los candidatos iniciales y en cada iteración del bucle de colapso. A los 60 segundos se activa la bandera, se descarta el resultado parcial y el modelo original queda como estaba.

### 10. Reducción en paralelo en el servidor

La reducción de una malla es secuencial, así que más núcleos no ayudan con una sola malla. Por eso el servidor usa dos niveles de paralelismo (`server/speedigon_server.cpp` y `mesh_chunking.cpp`):

- **Mallas pequeñas (menos de 100.000 triángulos):** hilos distintos procesan mallas distintas.
- **Mallas grandes:** la malla se corta en trozos de unos 25.000 triángulos (`triángulos / 25000`, con un mínimo de 2 y un máximo de 512) mediante bisección recursiva por la mediana de los centroides de las caras, a lo largo del eje más largo de su caja envolvente. Cada trozo recibe su propia lista de vértices y se reduce en paralelo con el **bloqueo de bordes forzado**, de modo que los vértices compartidos entre trozos no se muevan. Los resultados se unen identificando los vértices que tienen exactamente la misma posición y UV, se descartan las caras degeneradas y se recalculan todas las normales.

El costo es que las costuras entre trozos no se simplifican (alrededor del 3 % de los triángulos con trozos de 25.000 triángulos) y que los bordes reales del modelo quedan bloqueados aunque la opción esté desactivada.

### Referencias

- M. Garland y P. S. Heckbert. *Surface Simplification Using Quadric Error Metrics.* SIGGRAPH 1997.
- H. Hoppe. *Progressive Meshes.* SIGGRAPH 1996.
- T. K. Dey, H. Edelsbrunner, S. Guha, D. V. Nekhayev. *Topology Preserving Edge Contraction.* 1999 (la condición de enlace que usa `is_collapse_ok`).

## Limitaciones conocidas

- *Triángulos objetivo* y *Error máximo de deformación* todavía no los usa el algoritmo; solo el *Porcentaje de reducción* define el resultado.
- Las mallas se optimizan una por una, nunca entre mallas.
- Los bordes y las costuras UV bloqueados pueden impedir alcanzar la meta.
- La optimización local está limitada a 100.000 triángulos y 60 segundos.
- Speedigon usa núcleos de CPU, no GPU. Ayuda con mallas muy grandes, pero en modelos pequeños la transferencia por red puede hacerlo más lento que optimizar en local.
- La compra es simulada y `/purchase` otorga la marca a cualquier cuenta autenticada.
- La herramienta de línea de comandos de `main_cli.cpp` ignora su argumento de triángulos objetivo; usa el porcentaje de reducción por defecto.
- Solo macOS. OpenGL está obsoleto en macOS; todavía funciona, pero podría desaparecer en el futuro.

## Estructura del proyecto

| Archivo | Propósito |
| --- | --- |
| `main.cpp` | Aplicación, ventana, interfaz, importación/exportación y el trabajo de optimización. |
| `optimization_algorithms.h/.cpp` | El algoritmo de reducción descrito arriba. |
| `mesh_data.h` | Estructuras de datos de la malla. |
| `scene_renderer.h/.cpp` | Visor 3D: cielo, placa base, iluminación y dibujo del modelo. |
| `theme.h/.cpp` | Temas por defecto y neón. |
| `settings_ui.h/.cpp` | Ventanas de configuración, cuenta y Speedigon. |
| `localization.h/.cpp` | Traducciones (inglés, español, alemán e italiano). |
| `app_settings.h/.cpp` | Archivo local de ajustes. |
| `cloud_client.h/.cpp`, `cloud_config.example.h` | Cliente HTTP del backend y su configuración. |
| `mesh_codec.h/.cpp` | Codificación binaria y en base64 de las mallas para el servidor de optimización. |
| `mesh_chunking.h/.cpp` | Reducción paralela por trozos que usa el servidor. |
| `server/` | Servidor de optimización y su unidad de systemd. |
| `aws/lambda/` | Función Lambda de las cuentas. |
| `main_cli.cpp` | Herramienta mínima de línea de comandos. |
| `fonts/` | Fuentes que usa la interfaz. |
| `stb_image.h` | Carga de imágenes. |

## Notas de seguridad

- En la configuración por defecto se llega al servidor de optimización por HTTP sin cifrar, así que el token de sesión y la malla viajan sin cifrar. Ponlo detrás de HTTPS antes de usar cuentas reales.
- El token de sesión se guarda en texto plano en `~/.shrinkigon_settings`.
- Nunca subas `cloud_config.h`, claves privadas (`*.pem`) ni las variables de entorno de la Lambda (`TOKEN_SECRET`).
- Si despliegas el backend, configura una alerta de presupuesto en AWS y restringe el grupo de seguridad; un servidor de optimización público puede ser abusado.

## Software de terceros

Dear ImGui (MIT), GLFW (zlib/libpng), FreeType (licencia FreeType o GPLv2), Assimp (BSD-3-Clause), OpenMesh, libcurl (licencia curl), cpp-httplib (MIT) y stb_image (dominio público o MIT). Revisa la licencia de cada proyecto, incluida la de OpenMesh, para la versión exacta que redistribuyas. Las fuentes de `fonts/` tienen sus propias licencias.

## Licencia

Agrega aquí tu licencia (por ejemplo MIT) e incluye un archivo `LICENSE` en el repositorio.
