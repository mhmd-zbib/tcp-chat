#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include "types.h"
#include <pthread.h>
#include <time.h>

typedef struct {
    char            room_id[ROOM_ID_LENGTH + 1];
    char            room_name[MAX_ROOM_NAME];
    int             client_ids[MAX_CLIENTS_PER_ROOM];
    int             client_count;
    time_t          created_at;
    int             is_active;
    pthread_mutex_t mutex;
} room_t;

typedef struct {
    room_t          rooms[MAX_ROOMS];
    int             room_count;
    pthread_mutex_t global_mutex;
} room_manager_t;

// Room manager functions following FILENAME_FUNCTIONNAME pattern
void    room_manager_initialize(room_manager_t *manager);
void    room_manager_cleanup(room_manager_t *manager);
char   *room_manager_create_room(room_manager_t *manager, const char *room_name,
                                 int creator_client_id);
int     room_manager_join_room(room_manager_t *manager, const char *room_id, int client_id);
int     room_manager_leave_room(room_manager_t *manager, const char *room_id, int client_id);
int     room_manager_broadcast_to_room(room_manager_t *manager, const char *room_id,
                                       const char *message, int sender_id);
room_t *room_manager_find_room(room_manager_t *manager, const char *room_id);
void    room_manager_list_rooms(room_manager_t *manager, char *buffer, size_t buffer_size);
void    room_manager_get_room_users(room_manager_t *manager, const char *room_id, char *buffer,
                                    size_t buffer_size);

// Room-specific functions
void  room_initialize(room_t *room, const char *room_id, const char *room_name);
void  room_cleanup(room_t *room);
int   room_add_client(room_t *room, int client_id);
int   room_remove_client(room_t *room, int client_id);
int   room_has_client(room_t *room, int client_id);
char *room_generate_id(void);

#endif
