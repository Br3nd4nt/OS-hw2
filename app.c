#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SHM_NAME "/shm_pins"
#define SEM1_NAME "/sem_area1"
#define SEM2_NAME "/sem_area2"
#define SEM3_NAME "/sem_area3"

// Структура разделяемой памяти
typedef struct {
  int pins_area1;
  int pins_area2;
  int pins_area3;
} shared_data;

// Указатели на семафоры и разделяемую память
sem_t *sem_area1, *sem_area2, *sem_area3;
shared_data *shm_ptr;

// Обработчик сигналов для корректного завершения
void handle_sigint(int sig) {
  printf("Закрываем процесс...\n");

  // Удаление семафоров
  sem_close(sem_area1);
  sem_close(sem_area2);
  sem_close(sem_area3);
  sem_unlink(SEM1_NAME);
  sem_unlink(SEM2_NAME);
  sem_unlink(SEM3_NAME);

  // Удаление разделяемой памяти
  munmap(shm_ptr, sizeof(shared_data));
  shm_unlink(SHM_NAME);

  exit(0);
}

// Процесс участка 1
void area1_worker(int id) {
  while (1) {
    sleep(rand() % 3 + 1); // Проверка на кривизну (случайное время)
    printf("Работник %d из участке 1 проверяет булавк на кривизну.\n", id);

    sem_wait(sem_area1);
    if (shm_ptr->pins_area1 > 0) {
      shm_ptr->pins_area1--;
      sem_post(sem_area1);

      // 50% вероятность прохождения проверки
      if (rand() % 2 == 0) {
        sem_wait(sem_area2);
        shm_ptr->pins_area2++;
        sem_post(sem_area2);
        printf("Работник %d из участка 1 закончил проверку и передаёт булавку "
               "на участок 2.\n",
               id);
      } else {
        printf("Работник %d из участка 1 закончил проверку и булвка не прошла "
               "проверку.\n",
               id);
      }
    } else {
      sem_post(sem_area1);
    }
  }
}

// Процесс участка 2
void area2_worker(int id) {
  while (1) {
    sleep(rand() % 3 + 1); // Заточка (случайное время)
    printf("Работник %d из участка 2 затачивает булавку.\n", id);

    sem_wait(sem_area2);
    if (shm_ptr->pins_area2 > 0) {
      shm_ptr->pins_area2--;
      sem_post(sem_area2);

      sem_wait(sem_area3);
      shm_ptr->pins_area3++;
      sem_post(sem_area3);

      printf("Работник %d из участка 2 завершил заточку булавки и передаёт её "
             "на участок 3.\n",
             id);
    } else {
      sem_post(sem_area2);
    }
  }
}

// Процесс участка 3
void area3_worker(int id) {
  while (1) {
    sleep(rand() % 3 + 1); // Контроль качества (случайное время)
    printf("Работник %d из участка 3 проводит контроль качества.\n", id);

    sem_wait(sem_area3);
    if (shm_ptr->pins_area3 > 0) {
      shm_ptr->pins_area3--;
      sem_post(sem_area3);

      printf("Работник %d из участка 3 Завершил контроль качества.\n", id);
    } else {
      sem_post(sem_area3);
    }
  }
}

int main(int argc, char *argv[]) {
  int K = 3, L = 5, M = 2; // Количество рабочих на участках
  pid_t pids[K + L + M];

  // Инициализация случайного генератора
  srand(time(NULL));

  // Создание разделяемой памяти
  int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  ftruncate(shm_fd, sizeof(shared_data));
  shm_ptr = mmap(0, sizeof(shared_data), PROT_READ | PROT_WRITE, MAP_SHARED,
                 shm_fd, 0);

  // Инициализация разделяемой памяти
  shm_ptr->pins_area1 = 10; // Стартовое количество тупых булавок
  shm_ptr->pins_area2 = 0;
  shm_ptr->pins_area3 = 0;

  // Создание семафоров
  sem_area1 = sem_open(SEM1_NAME, O_CREAT, 0666, 1);
  sem_area2 = sem_open(SEM2_NAME, O_CREAT, 0666, 1);
  sem_area3 = sem_open(SEM3_NAME, O_CREAT, 0666, 1);

  // Установка обработчика сигнала для корректного завершения
  signal(SIGINT, handle_sigint);

  // Создание процессов для участка 1
  for (int i = 0; i < K; i++) {
    if ((pids[i] = fork()) == 0) {
      area1_worker(i);
      exit(0);
    }
  }

  // Создание процессов для участка 2
  for (int i = 0; i < L; i++) {
    if ((pids[K + i] = fork()) == 0) {
      area2_worker(i);
      exit(0);
    }
  }

  // Создание процессов для участка 3
  for (int i = 0; i < M; i++) {
    if ((pids[K + L + i] = fork()) == 0) {
      area3_worker(i);
      exit(0);
    }
  }

  // Ожидание завершения дочерних процессов
  for (int i = 0; i < K + L + M; i++) {
    waitpid(pids[i], NULL, 0);
  }

  // Удаление семафоров
  sem_close(sem_area1);
  sem_close(sem_area2);
  sem_close(sem_area3);
  sem_unlink(SEM1_NAME);
  sem_unlink(SEM2_NAME);
  sem_unlink(SEM3_NAME);

  // Удаление разделяемой памяти
  munmap(shm_ptr, sizeof(shared_data));
  shm_unlink(SHM_NAME);

  return 0;
}
