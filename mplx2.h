/* Multiplexor Library (c) 2004 Phillip Whelan
 * Multiplexor Library (c) 2005, 2006 Felipe Astroza
 *
 * Licensed under the LGPL
 * Con licencia LGPL
 */


#ifndef __MPLX_H_
#define __MPLX_H_

#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct mplx_list {
	struct mplx_socket *head;
	struct mplx_socket *tail;
	struct mplx_socket *cur;

	int num;
};


typedef int (*mplx_callback)(struct mplx_list *);
typedef void (*mplx_callback2)(struct mplx_socket *);

struct mplx_socket {
	/* descriptor de socket */
	int sockfd;
	
	/* el struct sockaddr de este socket */
	struct sockaddr *sa;
	
	/* el tamaño de sockaddr */
	socklen_t sa_size;
	
	/* el proximo mplx_socket en la lista */
	struct mplx_socket *next;
	
	/* el previo mplx_socket en la lista */
	struct mplx_socket *prev;
	
	/* el callback que se ejecutara sobre este socket */
	mplx_callback cb_recv;
	
	/* el callback que se ejecutara al eliminar este socket */
	mplx_callback2 cb_delete;

	/* Datos de la funcion encuestadora */
	void *data;

	/* Segundos apartir del ultimo evento */
	int timeout;

	/* Tiempo exacto de expiracion */
	struct timeval expires;

	/* Ultimo evento */
	int revents;
};

struct mplx_ops
{
	/* Funcion encargada de construir los datos necesarios para la funcion get_ev() */
	int (*build_func)(void **, struct mplx_socket **, int);

	/* Actualiza la lista de mplx_socket's con ultimos eventos */
	int (*get_ev)(void *); /* FIXME: Aviso seguro por incompatibles tipos de punteros */
};

struct mplx_handler {
	struct mplx_list list;
	struct mplx_socket *old_tail;
	struct mplx_socket **arr;
	void *data;

	struct mplx_ops *ops;

	int timeout;

	/* Numero de eventos */
	int ret;

	/* Uso interno */
	struct timeval c_time;
	int rebuild;
	int num_fd;
	int i;
};

/* When we use select() */

struct mplx_select_data {
	int highest;
	fd_set rfds;
	fd_set efds;
};

#define MPLX_HEAD(a)	((a)->head)
#define MPLX_TAIL(a)	((a)->tail)
#define MPLX_CUR(a)	((a)->cur)
#define MPLX_DATA(a)	((a)->cur->data)

#define MPLX_RECV_CALLBACK		0x03
#define MPLX_DELETE_SOCK_CALLBACK	0x05
#define MPLX_SET_DATA			0x07
#define MPLX_SET_TIMEOUT		0x09

/* Menor o igual a MPLX_DO_CLOSE */
#define MPLX_DO_CLOSE			0x0

#define MPLX_POLL_EVENT 		0x01
#define MPLX_POLL_ERROR 		0x02
#define MPLX_POLL_ALL			0x04

#define MPLX_ERROR 	-1
#define MPLX_OK         0
#define MPLX_FAILURE	1
#define MPLX_ONE_EVENT	2

#define MPLX_USE_POLL 	0
#define MPLX_USE_SELECT	1

#define MPLX_BUILD_FUNC(a, b) ((a)->ops->build_func(&(a)->data, (a)->arr, (b)))
#define MPLX_GET_EV(a) ((a)->ops->get_ev((a)))

static inline struct mplx_socket *MPLX_NEXT(struct mplx_list *list)
{
	if ( MPLX_CUR(list) == NULL )
		return(NULL);
	
	MPLX_CUR(list) = MPLX_CUR(list)->next;	
	return(MPLX_CUR(list));
}

static inline struct mplx_socket *MPLX_REWIND(struct mplx_list *list)
{
	MPLX_CUR(list) = MPLX_HEAD(list);
	return(MPLX_CUR(list));
}


struct mplx_socket *mplx_add_socket(struct mplx_list *, int, struct sockaddr *, socklen_t);
struct mplx_socket *mplx_listen_unix(struct mplx_list *, char *);
struct mplx_socket *mplx_listen_inet(struct mplx_list *, char *, unsigned short);
struct mplx_socket *mplx_connect_inet(struct mplx_list *, char *, unsigned short);
struct mplx_socket *mplx_connect_unix(struct mplx_list *, char *addr);
void mplx_set(struct mplx_socket *, int, void *);
int mplx_poll_event(struct mplx_handler *);
void mplx_close_conn(struct mplx_handler *, struct mplx_socket *);
int mplx_init(struct mplx_handler *, int, int);
void mplx_loop(struct mplx_handler *);

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
