
/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/apps/fs.h"
#include "string.h"
#include <stdio.h>
#include "httpserver.h"
#include "cmsis_os.h"

//Экспортируем переменные с main- файла
extern float Temperature, Pressure, Humidity;
extern uint8_t alert[3];
uint8_t datatest[50];
static void http_server(struct netconn *conn)
{
	struct netbuf *inbuf;
	err_t recv_err;
	char* buf;
	u16_t buflen;
	struct fs_file file;

	//Читаем данные с порта
	recv_err = netconn_recv(conn, &inbuf);

	if (recv_err == ERR_OK)
	{
		if (netconn_err(conn) == ERR_OK)
		{
			/*Получаем указатель на данные,его длину внутри netbuf */
			netbuf_data(inbuf, (void**)&buf, &buflen);
			/* Проверяем если запрос на определенный адрес */
			if (strncmp((char const *)buf,"GET /index.html",15)==0)
				{
					fs_open(&file, "/index.html");
					netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
					fs_close(&file);
				}
			if (strncmp((char const *)buf,"GET /chart.js",13)==0)
				{
					fs_open(&file, "/chart.js");
					netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
					fs_close(&file);
				}
			if (strncmp((char const *)buf,"GET /img/humidity.png",21)==0)
				{
					fs_open(&file, "/img/humidity.png");
					netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
					fs_close(&file);
				}
			if (strncmp((char const *)buf,"GET /img/pressure.png",21)==0)
				{
					fs_open(&file, "/img/pressure.png");
					netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
					fs_close(&file);
				}
			if (strncmp((char const *)buf,"GET /img/temperature.png",24)==0)
				{
					fs_open(&file, "/img/temperature.png");
					netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
					fs_close(&file);
				}
			//Отвечаем на запрос клиента о данных температуры, давления, влажности
			if (strncmp((char const *)buf,"GET /get_value",14)==0)
				{
					char *pagedata;
					pagedata = pvPortMalloc(10);
					int len = sprintf (pagedata, "%d %d %d", (int)Temperature, (int)Pressure, (int)Humidity);
					netconn_write(conn, (const unsigned char*)pagedata, (size_t)len, NETCONN_NOCOPY);
					vPortFree(pagedata);
				}

			//Получаем от клиента запрос с данными о выходе параметров за рамки установленных
			if (strncmp((char const *)buf,"GET /TEMP=",10)==0)
				{
					alert[0]=buf[10];
				}
			if (strncmp((char const *)buf,"GET /PRES=",10)==0)
				{
					alert[1]=buf[10];

				}
			if (strncmp((char const *)buf,"GET /HUMID=",11)==0)
				{
					alert[2]=buf[11];
				}
			else
			{
				fs_open(&file, "/404.html");
				netconn_write(conn, (const unsigned char*)(file.data), (size_t)file.len, NETCONN_NOCOPY);
				fs_close(&file);
			}
		}
	}
	/* Закрываем соединение(сервер закрыт в HTTP) */
	netconn_close(conn);

	/* Освобождаем массив */
	netbuf_delete(inbuf);
}


static void http_thread(void *arg)
{ 
  struct netconn *conn, *newconn;
  err_t err, accept_err;
  
  /* Создаем новый обработчик TCP соединения */
  conn = netconn_new(NETCONN_TCP);
  
  if (conn!= NULL)
  {
    /* Привязываем HTTP к порту 80 с обычным IP адресом*/
    err = netconn_bind(conn, IP_ADDR_ANY, 80);
    
    if (err == ERR_OK)
    {
      /*Ставим соединенение в состояние LISTEN */
      netconn_listen(conn);
  
      while(1) 
      {
        /* Принимаем любые входящие соединения*/
        accept_err = netconn_accept(conn, &newconn);
        if(accept_err == ERR_OK)
        {
          /* Соединение с сервером */
          http_server(newconn);

          /* Отсоединение с сервером */
          netconn_delete(newconn);
        }
      }
    }
  }
}



void http_server_init()
{
  sys_thread_new("http_thread", http_thread, NULL, DEFAULT_THREAD_STACKSIZE, osPriorityNormal);
}





