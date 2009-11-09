 /* Multiplexor Library (c) 2004 Phillip Whelan
 *  Multiplexor Library (c) 2005, 2006 Felipe Astroza
 * 
 * Licensed under the LGPL
 * Con licencia LGPL
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <sys/poll.h>
#include <mplx2.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef UNIX_MAX_PATH
#define UNIX_MAX_PATH 108
#endif
#include <sys/un.h>

/* F.A.: MPLX2 se divide en 2 capas, una de bajo nivel y otra alta.
 * En la primera nos encontramos con las funciones notificadores de eventos como poll(), select() y su implementacion.
 * La segunda es la que procesa la informacion llamando a las "callbacks" correspondientes y gestionando los "timeouts".
 */

static int build_pollfd(void **, struct mplx_socket **, int);
static int build_selectset(void **, struct mplx_socket **, int);
static int mplx_using_poll(struct mplx_handler *);
static int mplx_using_select(struct mplx_handler *);

static struct mplx_ops available_ops[] = {
		{&build_pollfd, &mplx_using_poll},
		{&build_selectset, &mplx_using_select}
};

struct mplx_socket *
mplx_add_socket(list, sockfd, sa, sa_size)
	struct mplx_list *list;
	int sockfd;
	struct sockaddr *sa;
	socklen_t sa_size;
{
	struct mplx_socket *new;
	struct mplx_socket *tail;
	
	assert(list);
	
	new = calloc(1, sizeof(struct mplx_socket));
	if ( new == NULL )
		return 0;
	
	if ((tail = MPLX_TAIL(list))) {
		tail->next = new;
		new->prev = tail;
	}
	else
		MPLX_HEAD(list) = new;
	
	MPLX_TAIL(list) = new;
	list->num++;
	
	new->sockfd = sockfd;
	new->sa = sa;
	new->sa_size = sa_size;
	
	return new;
}

static int mplx_del_socket(list, cur)
	struct mplx_list *list;
	struct mplx_socket *cur;
{
	struct mplx_socket *prev;
	struct mplx_socket *next;
	
	assert(list);

	if(cur->cb_delete)
		cur->cb_delete(cur);

	next = cur->next;
	prev = cur->prev;

	if (( cur == MPLX_HEAD(list)))
		MPLX_HEAD(list) = next;
	else if (( cur == MPLX_TAIL(list)))
		MPLX_TAIL(list) = prev;
	
	if (( cur->prev ))
		prev->next = next;
	if (( cur->next ))
		next->prev = prev;

	shutdown(cur->sockfd, SHUT_RDWR);
	close(cur->sockfd);

	free(cur);
	list->num--;

	return MPLX_OK;
}

struct mplx_socket *
mplx_listen_unix(list, addr)
	struct mplx_list *list;
	char *addr;
{
	struct sockaddr_un *sa_p;
	struct sockaddr_un sa;
	socklen_t sa_size;
	int sockfd;
	int ret;
	
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( sockfd == -1 )
		return NULL;
	
	memset((void *)&sa, 0, sizeof(sa));
	
	strncpy(sa.sun_path,addr,UNIX_MAX_PATH);
	sa.sun_path[UNIX_MAX_PATH-1] = '\0';
	sa.sun_family = AF_UNIX;
	sa_size = sizeof(sa.sun_family) + strlen(addr);

	ret = bind(sockfd, (struct sockaddr *)&sa, sa_size);
	if ( ret == -1 )
		return NULL;
	
	if ( listen(sockfd, 10) == -1)
		return NULL;
	
	sa_p = calloc(1, sa_size);
	memcpy(sa_p, (void *)&sa, sa_size);
	
	return(mplx_add_socket(list, sockfd, (struct sockaddr *)sa_p, sa_size));
}

struct mplx_socket *
mplx_listen_inet(list, addr, port)
	struct mplx_list *list;
	char *addr;
	unsigned short port;
{
	struct sockaddr_in sa, *sa_p;
	socklen_t sa_size;
	int sockfd;
	int ret;
        int status = 1;	
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd == -1 )
		return NULL;
	
	memset((void *)&sa, 0, sizeof(sa));
		
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
        sa.sin_addr.s_addr = inet_addr(addr);

	sa_size = sizeof(struct sockaddr_in);

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status)) == -1)
		return NULL;

	ret = bind(sockfd, (struct sockaddr *)&sa, sa_size);
	if ( ret == -1 )
		return NULL;
	
	if (listen(sockfd, 10) == -1)
		return NULL;
	
	sa_p = calloc(1, sa_size);
	memcpy(sa_p, (void *)&sa, sa_size);

	return(mplx_add_socket(list, sockfd, (struct sockaddr *)sa_p, sa_size));
}

struct mplx_socket *mplx_connect_inet(list, addr, port)
	struct mplx_list *list;
	char *addr;
	unsigned short port;
{
	struct sockaddr_in sa, *sa_p;
	struct hostent *host_s;
	socklen_t sa_size;
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
		return NULL;

	host_s = gethostbyname(addr);
	if(host_s == NULL)
		return NULL;

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ((int *)(host_s->h_addr_list[0]))[0];
	sa.sin_port = htons(port);

	sa_size = sizeof(struct sockaddr_in);
	memset(&sa.sin_zero, '\0', 8);

	if( connect(sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) == -1 )
		return NULL;

	sa_p = calloc(1, sa_size);
	memcpy(sa_p, (void *)&sa, sa_size);

	return(mplx_add_socket(list, sockfd, (struct sockaddr *)sa_p, sa_size));
}

struct mplx_socket *
mplx_connect_unix(list, addr)
	struct mplx_list *list;
	char *addr;
{
	struct sockaddr_un *sa_p;
	struct sockaddr_un sa;
	socklen_t sa_size;
	int sockfd;
	int ret;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( sockfd == -1 )
		return NULL;

	memset((void *)&sa, 0, sizeof(sa));

	strncpy(sa.sun_path,addr,UNIX_MAX_PATH);
	sa.sun_path[UNIX_MAX_PATH-1] = '\0';
	sa.sun_family = AF_UNIX;
	sa_size = sizeof(sa.sun_family) + strlen(addr);

	ret = connect(sockfd, (struct sockaddr *)&sa, sa_size);
	if ( ret == -1 )
		return NULL;

	sa_p = calloc(1, sa_size);
	memcpy(sa_p, (void *)&sa, sa_size);

	return(mplx_add_socket(list, sockfd, (struct sockaddr *)sa_p, sa_size));
}

/* SELECT() implementacion */

static void array_to_fdset(arr, s, nfds)
	struct mplx_socket **arr;
	struct mplx_select_data *s;
	int nfds;
{
	int i;

	FD_ZERO(&s->rfds);
	FD_ZERO(&s->efds);
	s->highest = 0;

	for(i=0; i < nfds; i++) {
		FD_SET(arr[i]->sockfd, &s->rfds);
		FD_SET(arr[i]->sockfd, &s->efds);

		if(arr[i]->sockfd > s->highest)
			s->highest = arr[i]->sockfd;
	}
}

static int build_selectset(data, arr, nfds)
	void **data;
	struct mplx_socket **arr;
	int nfds;
{
	if(*data == NULL) {
		*data = malloc(sizeof(struct mplx_select_data));
		if(*data == NULL)
			return MPLX_ERROR;
	}

	array_to_fdset(arr, (struct mplx_select_data *)*data, nfds);

	return MPLX_OK;
}

static int mplx_using_select(handler)
	struct mplx_handler *handler;
{
	struct mplx_list *list;
	struct mplx_socket **arr;
	struct mplx_select_data *s;
	int ret=0, i;
	struct timeval tv;


	list = &handler->list;
	arr = handler->arr;
	s = handler->data;

	tv.tv_sec = handler->timeout;
	tv.tv_usec = 0;

	if(select(s->highest + 1, &s->rfds, NULL, &s->efds, &tv) < 0)
		return MPLX_ERROR;

	for(i=0; i < list->num; i++) {
		arr[i]->revents = 0;
		if(FD_ISSET(arr[i]->sockfd, &s->rfds)) {
			ret++;
			arr[i]->revents |= MPLX_POLL_EVENT;
		}

		/* Excepciones: errores */
		if(FD_ISSET(arr[i]->sockfd, &s->efds)) {
			ret++;
			arr[i]->revents |= MPLX_POLL_ERROR;
		}
	}

	/* Agregando nuevamente los descriptores a el fd_set */
	array_to_fdset(arr, s, list->num);

	return ret;
}

/* ------------------------------------------------------------------------------------ */

/* POLL() implementacion */

/* Construye el array de struct pollfd */
static int build_pollfd(data, arr, nfds)
	void **data;
	struct mplx_socket **arr;
	int nfds;
{
	int i;

	if(*data != NULL)
		free(*data);

	*data = calloc(nfds, sizeof(struct pollfd));

	if(*data == NULL)
		return MPLX_ERROR;

	for(i=0; i < nfds; i++) {
		((struct pollfd *)(*data))[i].fd = arr[i]->sockfd;
		((struct pollfd *)(*data))[i].events = POLLIN|POLLHUP|POLLNVAL|POLLERR;
	}

	return MPLX_OK;
}

/* Obtiene los eventos usando "poll()" */
static int mplx_using_poll(handler)
	struct mplx_handler *handler;
{
	struct mplx_list *list;
	struct mplx_socket **arr;
	struct pollfd *p_fd;
	int ret, i;

	list = &handler->list;
	arr = handler->arr;
	p_fd = handler->data;
	ret = poll(p_fd, list->num, handler->timeout);

	/* ERROR */
	if(ret < 0)
		return MPLX_ERROR;

	if(ret == 0)
		return 0;

	for(i=0; i < list->num; i++) {

		arr[i]->revents = 0;
		if(p_fd[i].revents & POLLIN)
			arr[i]->revents |= MPLX_POLL_EVENT;

		if(p_fd[i].revents & (POLLHUP|POLLNVAL|POLLERR))
			arr[i]->revents |= MPLX_POLL_ERROR;
	}

	return ret;
}
		
/* ---------------------------------------------------------------- */
	
void mplx_set(conn, what, addr)
	struct mplx_socket *conn;
	int what;
	void *addr;
{
	struct timeval tv;

	if(conn != NULL) {
		switch(what) {

			case MPLX_RECV_CALLBACK:

				conn->cb_recv = (mplx_callback)addr;
				break;

			case MPLX_DELETE_SOCK_CALLBACK:

				conn->cb_delete = (mplx_callback2)addr;
				break;

			case MPLX_SET_DATA:

				conn->data = addr;
				break;

			case MPLX_SET_TIMEOUT:
				/* este timeout tiene que ser multiplo de timeout de la funcion
				 * encuestadora de eventos
				 */
				conn->timeout = (int)addr;
				gettimeofday(&tv, NULL);
				conn->expires.tv_sec = tv.tv_sec + conn->timeout;
				conn->expires.tv_usec = tv.tv_usec;
				break;
		}
	}
}

void mplx_close_conn(handler, conn)
	struct mplx_handler *handler;
	struct mplx_socket *conn;
{
	if(conn) {
		mplx_del_socket(&handler->list, conn);
		handler->rebuild = 1;
	}
}

/* Pasa la lista de mplx_socket a un array, luego llama al callback
   asignado que construye los datos necesario para la funcion notificadora de eventos */
static int build_mplx_array(handler)
	struct mplx_handler *handler;
{
	struct mplx_list *list = &handler->list;
	struct mplx_socket *cur;
	int i;

	handler->arr = (list->num == 0)? NULL : calloc(list->num, sizeof(struct mplx_socket *));
	if (handler->arr == NULL )
		return MPLX_ERROR;

	for(i = 0, cur = MPLX_REWIND(list); cur; i++, cur = MPLX_NEXT(list))
		handler->arr[i] = cur;

	if(MPLX_BUILD_FUNC(handler, list->num) == MPLX_ERROR) {
		free(handler->arr);
		return MPLX_ERROR;
	}

	return list->num;
}

int mplx_poll_event(handler)
	struct mplx_handler *handler;
{
	int i;
	struct mplx_list *list;
	struct mplx_socket **arr, *cur, *next;

	if (handler == NULL)
		return MPLX_ERROR;

	list = &handler->list;

	/* Inicio */
	if(handler->i == 0) {

		/* Revisa los timeouts */
		gettimeofday(&handler->c_time, NULL);
		cur = MPLX_REWIND(list);
		while(cur != NULL) {
			next = cur->next;
			if(cur->timeout > 0 && timercmp(&handler->c_time, &cur->expires, >)) {
				mplx_del_socket(list, cur);
				handler->rebuild = 1;
			}
			cur = next;
		}

		if (handler->rebuild != 0) {
			if(handler->arr)
				free(handler->arr);

			if (build_mplx_array(handler) == 0)
				return MPLX_ERROR;

			handler->rebuild = 0;
		}

		assert(handler->ops);

		handler->ret = MPLX_GET_EV(handler);

		/* ERROR */
		if (handler->ret == MPLX_ERROR)
			return MPLX_ERROR;

		/* ningun evento */
		if (handler->ret == 0)
			return MPLX_OK;

	       /* guardamos el tail viejo para agregar nuevos sockets
		* despues de procesar las peticiones
 		*/

		handler->num_fd = list->num;
		handler->old_tail = MPLX_TAIL(list);

	}

	for (arr = handler->arr, i = handler->i; handler->ret > 0 && i < handler->num_fd; i++) {
		if ((arr[i]->revents == 0))
			continue;

		if ((arr[i]->revents & MPLX_POLL_EVENT)) {
			handler->ret--;

			/* Existe un evento, refrescamos su expiracion */
			MPLX_CUR(list) = arr[i];
			MPLX_CUR(list)->expires.tv_sec = handler->c_time.tv_sec + MPLX_CUR(list)->timeout;
			MPLX_CUR(list)->expires.tv_usec = handler->c_time.tv_usec;

			/* incrementamos el indice y lo guardamos */
			handler->i = ++i;

			return MPLX_ONE_EVENT;

		/* Error encontrado */
		} else if ((arr[i]->revents & MPLX_POLL_ERROR)) {
			mplx_del_socket(list, arr[i]);
			handler->rebuild = 1;
		}
	}

	if ( handler->old_tail->next )
		handler->rebuild = 1;

	/* rebobinando */ 
	handler->i = 0;

	return MPLX_OK;
}

int mplx_init(handler, use, timeout)
	struct mplx_handler *handler;
	int use;
	int timeout;
{
	if(!handler)
		return MPLX_FAILURE;

	switch(use) {
		case MPLX_USE_POLL:
			handler->ops = &available_ops[MPLX_USE_POLL];
			break;
		case MPLX_USE_SELECT:
			handler->ops = &available_ops[MPLX_USE_SELECT];
			break;
		default:
			return MPLX_FAILURE;
	}

	MPLX_HEAD(&handler->list) = NULL;
	MPLX_CUR(&handler->list) = NULL;
	MPLX_TAIL(&handler->list) = NULL;
	handler->list.num = 0;
	handler->old_tail = NULL;
	handler->arr = NULL;
	handler->data = NULL;
	handler->ret = 0;
	handler->rebuild = 1;
	handler->num_fd = 0;
	handler->i = 0;
	handler->timeout = timeout;

	signal(SIGPIPE, SIG_IGN);

	return MPLX_OK;
}

void mplx_loop(handler)
	struct mplx_handler *handler;
{
	int loop = 1;
	struct mplx_list *list = &handler->list;

	while(loop) {
		switch( mplx_poll_event(handler) ) {
			case MPLX_ERROR:
				loop = 0;
				break;
			case MPLX_OK:
				break;
			case MPLX_ONE_EVENT:
				if( MPLX_CUR(list)->cb_recv(list) <= MPLX_DO_CLOSE )
					mplx_close_conn(handler, MPLX_CUR(list));
				break;
		}
	}
}
