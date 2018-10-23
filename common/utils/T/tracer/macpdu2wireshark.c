#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "database.h"
#include "event.h"
#include "handler.h"
#include "config.h"
#include "utils.h"
#include "packet-mac-lte.h"

#define DEFAULT_IP   "127.0.0.1"
#define DEFAULT_PORT 9999

int no_sib = 0;

typedef struct {
  int socket;
  struct sockaddr_in to;
  OBUF buf;
  /* ul */
  int ul_rnti;
  int ul_frame;
  int ul_subframe;
  int ul_data;
  /* dl */
  int dl_rnti;
  int dl_frame;
  int dl_subframe;
  int dl_data;
  /* mib */
  int mib_frame;
  int mib_subframe;
  int mib_data;
  /* RA preamble */
  int preamble_frame;
  int preamble_subframe;
  int preamble_preamble;
  /* RAR */
  int rar_rnti;
  int rar_frame;
  int rar_subframe;
  int rar_data;
} ev_data;

void ul(void *_d, event e)
{
  ev_data *d = _d;
  ssize_t ret;
  int fsf;
  int i;

  d->buf.osize = 0;

  PUTS(&d->buf, MAC_LTE_START_STRING);
  PUTC(&d->buf, FDD_RADIO);
  PUTC(&d->buf, DIRECTION_UPLINK);
  PUTC(&d->buf, C_RNTI);

  PUTC(&d->buf, MAC_LTE_RNTI_TAG);
  PUTC(&d->buf, (e.e[d->ul_rnti].i>>8) & 255);
  PUTC(&d->buf, e.e[d->ul_rnti].i & 255);

  /* for newer version of wireshark? */
  fsf = (e.e[d->ul_frame].i << 4) + e.e[d->ul_subframe].i;
  /* for older version? */
  fsf = e.e[d->ul_subframe].i;
  PUTC(&d->buf, MAC_LTE_FRAME_SUBFRAME_TAG);
  PUTC(&d->buf, (fsf>>8) & 255);
  PUTC(&d->buf, fsf & 255);

  PUTC(&d->buf, MAC_LTE_PAYLOAD_TAG);
  for (i = 0; i < e.e[d->ul_data].bsize; i++)
    PUTC(&d->buf, ((char*)e.e[d->ul_data].b)[i]);

  ret = sendto(d->socket, d->buf.obuf, d->buf.osize, 0,
      (struct sockaddr *)&d->to, sizeof(struct sockaddr_in));
  if (ret != d->buf.osize) abort();
}

void dl(void *_d, event e)
{
  ev_data *d = _d;
  ssize_t ret;
  int fsf;
  int i;

  if (e.e[d->dl_rnti].i == 0xffff && no_sib) return;

  d->buf.osize = 0;

  PUTS(&d->buf, MAC_LTE_START_STRING);
  PUTC(&d->buf, FDD_RADIO);
  PUTC(&d->buf, DIRECTION_DOWNLINK);
  if (e.e[d->dl_rnti].i != 0xffff) {
    PUTC(&d->buf, C_RNTI);

    PUTC(&d->buf, MAC_LTE_RNTI_TAG);
    PUTC(&d->buf, (e.e[d->dl_rnti].i>>8) & 255);
    PUTC(&d->buf, e.e[d->dl_rnti].i & 255);
  } else {
    PUTC(&d->buf, SI_RNTI);
  }

  /* for newer version of wireshark? */
  fsf = (e.e[d->dl_frame].i << 4) + e.e[d->dl_subframe].i;
  /* for older version? */
  //fsf = e.e[d->dl_subframe].i;
  PUTC(&d->buf, MAC_LTE_FRAME_SUBFRAME_TAG);
  PUTC(&d->buf, (fsf>>8) & 255);
  PUTC(&d->buf, fsf & 255);

  PUTC(&d->buf, MAC_LTE_PAYLOAD_TAG);
  for (i = 0; i < e.e[d->dl_data].bsize; i++)
    PUTC(&d->buf, ((char*)e.e[d->dl_data].b)[i]);

  ret = sendto(d->socket, d->buf.obuf, d->buf.osize, 0,
      (struct sockaddr *)&d->to, sizeof(struct sockaddr_in));
  if (ret != d->buf.osize) abort();
}

void mib(void *_d, event e)
{
  ev_data *d = _d;
  ssize_t ret;
  int fsf;
  int i;

  d->buf.osize = 0;

  PUTS(&d->buf, MAC_LTE_START_STRING);
  PUTC(&d->buf, FDD_RADIO);
  PUTC(&d->buf, DIRECTION_DOWNLINK);
  PUTC(&d->buf, NO_RNTI);

  /* for newer version of wireshark? */
  fsf = (e.e[d->mib_frame].i << 4) + e.e[d->mib_subframe].i;
  /* for older version? */
  //fsf = e.e[d->mib_subframe].i;
  PUTC(&d->buf, MAC_LTE_FRAME_SUBFRAME_TAG);
  PUTC(&d->buf, (fsf>>8) & 255);
  PUTC(&d->buf, fsf & 255);

  PUTC(&d->buf, MAC_LTE_PAYLOAD_TAG);
  for (i = 0; i < e.e[d->mib_data].bsize; i++)
    PUTC(&d->buf, ((char*)e.e[d->mib_data].b)[i]);

  ret = sendto(d->socket, d->buf.obuf, d->buf.osize, 0,
      (struct sockaddr *)&d->to, sizeof(struct sockaddr_in));
  if (ret != d->buf.osize) abort();
}

void preamble(void *_d, event e)
{
  ev_data *d = _d;
  ssize_t ret;
  int fsf;

  d->buf.osize = 0;

  PUTS(&d->buf, MAC_LTE_START_STRING);
  PUTC(&d->buf, FDD_RADIO);
  PUTC(&d->buf, DIRECTION_UPLINK);

  PUTC(&d->buf, NO_RNTI);

  /* for newer version of wireshark? */
  fsf = (e.e[d->preamble_frame].i << 4) + e.e[d->preamble_subframe].i;
  /* for older version? */
  //fsf = e.e[d->preamble_subframe].i;
  PUTC(&d->buf, MAC_LTE_FRAME_SUBFRAME_TAG);
  PUTC(&d->buf, (fsf>>8) & 255);
  PUTC(&d->buf, fsf & 255);

  PUTC(&d->buf, MAC_LTE_SEND_PREAMBLE_TAG);
  PUTC(&d->buf, e.e[d->preamble_preamble].i);
  PUTC(&d->buf, 0); /* rach attempt - always 0 for us (not sure of this) */

  /* if we don't put this (even with no data) wireshark (2.4.5) is not happy */
  PUTC(&d->buf, MAC_LTE_PAYLOAD_TAG);

  ret = sendto(d->socket, d->buf.obuf, d->buf.osize, 0,
      (struct sockaddr *)&d->to, sizeof(struct sockaddr_in));
  if (ret != d->buf.osize) abort();
}

void rar(void *_d, event e)
{
  ev_data *d = _d;
  ssize_t ret;
  int fsf;
  int i;

  if (e.e[d->rar_rnti].i == 0xffff && no_sib) return;

  d->buf.osize = 0;

  PUTS(&d->buf, MAC_LTE_START_STRING);
  PUTC(&d->buf, FDD_RADIO);
  PUTC(&d->buf, DIRECTION_DOWNLINK);
  PUTC(&d->buf, RA_RNTI);

  PUTC(&d->buf, MAC_LTE_RNTI_TAG);
  PUTC(&d->buf, (e.e[d->rar_rnti].i>>8) & 255);
  PUTC(&d->buf, e.e[d->rar_rnti].i & 255);

  /* for newer version of wireshark? */
  fsf = (e.e[d->rar_frame].i << 4) + e.e[d->rar_subframe].i;
  /* for older version? */
  //fsf = e.e[d->rar_subframe].i;
  PUTC(&d->buf, MAC_LTE_FRAME_SUBFRAME_TAG);
  PUTC(&d->buf, (fsf>>8) & 255);
  PUTC(&d->buf, fsf & 255);

  PUTC(&d->buf, MAC_LTE_PAYLOAD_TAG);
  for (i = 0; i < e.e[d->rar_data].bsize; i++)
    PUTC(&d->buf, ((char*)e.e[d->rar_data].b)[i]);

  ret = sendto(d->socket, d->buf.obuf, d->buf.osize, 0,
      (struct sockaddr *)&d->to, sizeof(struct sockaddr_in));
  if (ret != d->buf.osize) abort();
}

void setup_data(ev_data *d, void *database, int ul_id, int dl_id, int mib_id,
    int preamble_id, int rar_id)
{
  database_event_format f;
  int i;

  d->ul_rnti           = -1;
  d->ul_frame          = -1;
  d->ul_subframe       = -1;
  d->ul_data           = -1;

  d->dl_rnti           = -1;
  d->dl_frame          = -1;
  d->dl_subframe       = -1;
  d->dl_data           = -1;

  d->mib_frame         = -1;
  d->mib_subframe      = -1;
  d->mib_data          = -1;

  d->preamble_frame    = -1;
  d->preamble_subframe = -1;
  d->preamble_preamble = -1;

  d->rar_rnti           = -1;
  d->rar_frame          = -1;
  d->rar_subframe       = -1;
  d->rar_data           = -1;

#define G(var_name, var_type, var) \
  if (!strcmp(f.name[i], var_name)) { \
    if (strcmp(f.type[i], var_type)) goto error; \
    var = i; \
    continue; \
  }

  /* ul: rnti, frame, subframe, data */
  f = get_format(database, ul_id);
  for (i = 0; i < f.count; i++) {
    G("rnti",     "int",    d->ul_rnti);
    G("frame",    "int",    d->ul_frame);
    G("subframe", "int",    d->ul_subframe);
    G("data",     "buffer", d->ul_data);
  }
  if (d->ul_rnti == -1 || d->ul_frame == -1 || d->ul_subframe == -1 ||
      d->ul_data == -1) goto error;

  /* dl: rnti, frame, subframe, data */
  f = get_format(database, dl_id);
  for (i = 0; i < f.count; i++) {
    G("rnti",     "int",    d->dl_rnti);
    G("frame",    "int",    d->dl_frame);
    G("subframe", "int",    d->dl_subframe);
    G("data",     "buffer", d->dl_data);
  }
  if (d->dl_rnti == -1 || d->dl_frame == -1 || d->dl_subframe == -1 ||
      d->dl_data == -1) goto error;

  if (mib_id != -1) {
    /* MIB: frame, subframe, data */
    f = get_format(database, mib_id);
    for (i = 0; i < f.count; i++) {
      G("frame",    "int",    d->mib_frame);
      G("subframe", "int",    d->mib_subframe);
      G("data",     "buffer", d->mib_data);
    }
    if (d->mib_frame == -1 || d->mib_subframe == -1 || d->mib_data == -1)
      goto error;
  }

  /* preamble: frame, subframe, preamble */
  f = get_format(database, preamble_id);
  for (i = 0; i < f.count; i++) {
    G("frame",    "int", d->preamble_frame);
    G("subframe", "int", d->preamble_subframe);
    G("preamble", "int", d->preamble_preamble);
  }
  if (d->preamble_frame == -1 || d->preamble_subframe == -1 ||
      d->preamble_preamble == -1) goto error;

  /* rar: rnti, frame, subframe, data */
  f = get_format(database, rar_id);
  for (i = 0; i < f.count; i++) {
    G("rnti",     "int",    d->rar_rnti);
    G("frame",    "int",    d->rar_frame);
    G("subframe", "int",    d->rar_subframe);
    G("data",     "buffer", d->rar_data);
  }
  if (d->rar_rnti == -1 || d->rar_frame == -1 || d->rar_subframe == -1 ||
      d->rar_data == -1) goto error;

#undef G

  return;

error:
  printf("bad T_messages.txt\n");
  abort();
}

void *receiver(void *_d)
{
  ev_data *d = _d;
  int s;
  char buf[100000];

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1) { perror("socket"); abort(); }

  if (bind(s, (struct sockaddr *)&d->to, sizeof(struct sockaddr_in)) == -1)
    { perror("bind"); abort(); }

  while (1) {
    if (recv(s, buf, 100000, 0) <= 0) abort();
  }

  return 0;
}

void usage(void)
{
  printf(
"options:\n"
"    -d <database file>        this option is mandatory\n"
"    -i <dump file>            read events from this dump file\n"
"    -ip <IP address>          send packets to this IP address (default %s)\n"
"    -p <port>                 send packets to this port (default %d)\n"
"    -no-mib                   do not report MIB\n"
"    -no-sib                   do not report SIBs\n",
  DEFAULT_IP,
  DEFAULT_PORT
  );
  exit(1);
}

int main(int n, char **v)
{
  char *database_filename = NULL;
  char *input_filename = NULL;
  void *database;
  event_handler *h;
  int in;
  int i;
  int ul_id, dl_id, mib_id = -1, preamble_id, rar_id;
  ev_data d;
  char *ip = DEFAULT_IP;
  int port = DEFAULT_PORT;
  int do_mib = 1;

  memset(&d, 0, sizeof(ev_data));

  for (i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-d"))
      { if (i > n-2) usage(); database_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-i"))
      { if (i > n-2) usage(); input_filename = v[++i]; continue; }
    if (!strcmp(v[i], "-ip")) { if (i > n-2) usage(); ip = v[++i]; continue; }
    if (!strcmp(v[i], "-p")) {if(i>n-2)usage(); port=atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-no-mib")) { do_mib = 0; continue; }
    if (!strcmp(v[i], "-no-sib")) { no_sib = 1; continue; }
    usage();
  }

  if (database_filename == NULL) {
    printf("ERROR: provide a database file (-d)\n");
    exit(1);
  }

  if (input_filename == NULL) {
    printf("ERROR: provide an input file (-i)\n");
    exit(1);
  }

  in = open(input_filename, O_RDONLY);
  if (in == -1) { perror(input_filename); return 1; }

  database = parse_database(database_filename);
  load_config_file(database_filename);

  h = new_handler(database);

  ul_id = event_id_from_name(database, "ENB_MAC_UE_UL_PDU_WITH_DATA");
  dl_id = event_id_from_name(database, "ENB_MAC_UE_DL_PDU_WITH_DATA");
  if (do_mib) mib_id = event_id_from_name(database, "ENB_PHY_MIB");
  preamble_id=event_id_from_name(database, "ENB_PHY_INITIATE_RA_PROCEDURE");
  rar_id = event_id_from_name(database, "ENB_MAC_UE_DL_RAR_PDU_WITH_DATA");
  setup_data(&d, database, ul_id, dl_id, mib_id, preamble_id, rar_id);

  register_handler_function(h, ul_id, ul, &d);
  register_handler_function(h, dl_id, dl, &d);
  if (do_mib) register_handler_function(h, mib_id, mib, &d);
  register_handler_function(h, preamble_id, preamble, &d);
  register_handler_function(h, rar_id, rar, &d);

  d.socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (d.socket == -1) { perror("socket"); exit(1); }

  d.to.sin_family = AF_INET;
  d.to.sin_port = htons(port);
  d.to.sin_addr.s_addr = inet_addr(ip);

  new_thread(receiver, &d);

  OBUF ebuf = { osize: 0, omaxsize: 0, obuf: NULL };

  /* read messages */
  while (1) {
    event e;
    e = get_event(in, &ebuf, database);
    if (e.type == -1) break;
    if (!(e.type == ul_id || e.type == dl_id || e.type == mib_id ||
          e.type == preamble_id || e.type == rar_id)) continue;
    handle_event(h, e);
  }

  return 0;
}
