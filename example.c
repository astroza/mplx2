/* Multiplexor Library (c) 2006 Felipe Astroza
 * 
 * Licensed under the LGPL
 * Con licensia LGPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <mplx2.h>

#define MPLX_FUNCTION MPLX_USE_POLL

void disconnected(struct mplx_socket *mplx)
{
	printf("client disconnected\n");

}

int echo(struct mplx_list *list)
{
	struct mplx_socket *cur = MPLX_CUR(list);
	char buf[256];
	int ret;
        const char autor[]="TEST Multiplexor Library by MPLX TEAM 2005\n";
	
	ret = recv(cur->sockfd, buf, sizeof(buf), 0);

	if ( ret <= 0 )
		return 0;

	if(buf[0] == '\n' || buf[0] == '\r')
		return 0;

	send(cur->sockfd, autor, sizeof(autor), 0);
	send(cur->sockfd, buf, ret, 0);

	return 1;
}


int accept_mplx(struct mplx_list *list)
{
	int clifd;
	struct sockaddr *sa;
	struct mplx_socket *cur = MPLX_CUR(list), *mplx;
	int sa_size;
	
	sa_size = cur->sa_size;

	sa = calloc(1, sa_size);
	clifd = accept(cur->sockfd, sa, (socklen_t *)&sa_size);
	if ( clifd == -1 )
		return -1;

	mplx = mplx_add_socket(list, clifd, (struct sockaddr *)sa, sa_size);

	mplx_set(mplx, MPLX_RECV_CALLBACK, (void *)&echo);
	mplx_set(mplx, MPLX_DELETE_SOCK_CALLBACK, (void *)&disconnected);
	mplx_set(mplx, MPLX_SET_TIMEOUT, (void *)20);

	return 1;
}

int main()
{
	struct mplx_handler h;

	mplx_init(&h, MPLX_FUNCTION, 5000);

	mplx_set(mplx_listen_inet(&h.list, "0.0.0.0", 2500), MPLX_RECV_CALLBACK, (void *)&accept_mplx);
	mplx_loop(&h);

	return 0;
}
