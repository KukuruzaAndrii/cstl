/* *****************************************************************************
Copyright: Boaz Segev, 2022
License: ISC / MIT (choose your license)

Feel free to copy, use and enjoy according to the license provided.
***************************************************************************** */

/* *****************************************************************************
This is a simple chat server example, exploring pub/sub on plain text protocol.
***************************************************************************** */

#define FIO_MEMORY_DISABLE 1
// #define FIO_USE_THREAD_MUTEX 1
// #define FIO_POLL_ENGINE FIO_POLL_ENGINE_POLL
#define FIO_LEAK_COUNTER 1
#define FIO_EVERYTHING
#include "fio-stl/include.h"

/* *****************************************************************************
Callbacks and object used by main()
***************************************************************************** */

/** Called when a new connection is created. */
FIO_SFUNC void on_open(int fd, void *udata);
/** Called when a login process should be performed. */
FIO_SFUNC void on_login_start(fio_s *io);
/** Called there's incoming data (from STDIN / the client socket. */
FIO_SFUNC void on_data_login(fio_s *io);
FIO_SFUNC void on_data_chat(fio_s *io);
/** Called when a login process should be performed. */
FIO_SFUNC void on_shutdown(fio_s *io);
/** Called when the monitored IO is closed or has a fatal error. */
FIO_SFUNC void on_close(void *udata);

static fio_protocol_s CHAT_PROTOCOL_LOGIN = {
    .on_attach = on_login_start,
    .on_data = on_data_login,
    .on_close = on_close,
};
static fio_protocol_s CHAT_PROTOCOL_CHAT = {
    .on_data = on_data_chat,
    .on_close = on_close,
    .on_shutdown = on_shutdown,
};

/* *****************************************************************************
IO "Objects" and helpers
***************************************************************************** */

#define CHAT_MAX_HANDLE_LEN  31 /* must be less than 254 */
#define CHAT_MAX_MESSAGE_LEN 8192

typedef struct {
  fio_stream_s input;
  char name[CHAT_MAX_HANDLE_LEN + 1];
} client_s;

FIO_IFUNC client_s *client_new(void) {
  client_s *c = fio_calloc(sizeof(*c), 1);
  c->input = (fio_stream_s)FIO_STREAM_INIT(c->input);
  return c;
}

FIO_IFUNC void client_free(client_s *c) {
  fio_stream_destroy(&c->input);
  fio_free(c);
}

/** Called when a new connection is created. */
FIO_SFUNC void on_open(int fd, void *udata) {
  fio_s *io = fio_srv_attach_fd(fd, &CHAT_PROTOCOL_LOGIN, client_new(), udata);
  if (fio_cli_get_bool("-v"))
    FIO_LOG_INFO("(%d) %p connected", getpid(), (void *)io);
}

/** Called when a login process should be performed. */
FIO_SFUNC void on_login_start(fio_s *io) {
  fio_write(io, "Please enter a login handle (up to 30 characters long)\n", 55);
}

/** Called when the monitored IO is closed or has a fatal error. */
FIO_SFUNC void on_close(void *udata) {
  FIO_STR_INFO_TMP_VAR(s, CHAT_MAX_HANDLE_LEN + 32);
  client_s *c = udata;
  fio_string_write2(&s,
                    NULL,
                    FIO_STRING_WRITE_STR2(c->name, c->name[31]),
                    FIO_STRING_WRITE_STR1(" left the chat.\n"));
  fio_publish(.message = FIO_STR2BUF_INFO(s));
  client_free(c);
}

/** Performs "login" logic (saves user handle) */
FIO_SFUNC void on_data_first_line(fio_s *io, char *name, size_t len) {
  if (len < 2)
    goto error_name_too_short;
  --len;
  if (len > 30)
    goto error_name_too_long;
  client_s *c = fio_udata_get(io);
  memcpy(c->name, name, len);
  c->name[len] = 0;
  c->name[31] = (char)len;
  char *welcome =
      fio_bstr_write2(NULL,
                      FIO_STRING_WRITE_STR1("Welcome, "),
                      FIO_STRING_WRITE_STR2(name, len),
                      FIO_STRING_WRITE_STR1(", to the chat server.\n"),
                      FIO_STRING_WRITE_STR1("You are connected to node: "),
                      FIO_STRING_WRITE_UNUM(getpid()),
                      FIO_STRING_WRITE_STR1("\n"));
  fio_write2(io,
             .buf = welcome,
             .len = fio_bstr_len(welcome),
             .dealloc = (void (*)(void *))fio_bstr_free);
  return;
error_name_too_long:
  fio_write(io, "ERROR! login handle too long. Goodbye.\n", 39);
  fio_close(io);
  return;
error_name_too_short:
  fio_write(io, "ERROR! login handle too short (empty?). Goodbye.\n", 49);
  fio_close(io);
}

/** Manages chat messages */
FIO_SFUNC void on_data_message_line(fio_s *io, char *msg, size_t len) {
  client_s *c = fio_udata_get(io);
  char *buf = fio_bstr_write2(NULL,
                              FIO_STRING_WRITE_STR2(c->name, c->name[31]),
                              FIO_STRING_WRITE_STR2(": ", 2),
                              FIO_STRING_WRITE_STR2(msg, len));
  fio_publish(.message = fio_bstr_buf(buf), /* .from = io */);
  fio_bstr_free(buf);
  if (((len == 4 || len == 5) &&
       (((msg[0] | 32) == 'b') & ((msg[1] | 32) == 'y') &
        ((msg[2] | 32) == 'e'))) ||
      ((len == 8 || len == 9) &&
       (((msg[0] | 32) == 'g') & ((msg[1] | 32) == 'o') &
        ((msg[2] | 32) == 'o') & ((msg[3] | 32) == 'd') &
        ((msg[4] | 32) == 'b') & ((msg[5] | 32) == 'y') &
        ((msg[6] | 32) == 'e')))) {
    fio_write(io, "Goodbye.\n", 9);
    fio_close(io);
  }
}

FIO_IFUNC int on_data_read(fio_s *io) {
  char buf[CHAT_MAX_MESSAGE_LEN];
  size_t r = fio_read(io, buf, CHAT_MAX_MESSAGE_LEN);
  if (!r)
    return -1;
  client_s *c = fio_udata_get(io);
  fio_stream_add(&c->input, fio_stream_pack_data(buf, r, 0, 1, NULL));
  return 0;
}

FIO_IFUNC int on_data_process_line(fio_s *io,
                                   void(task)(fio_s *, char *, size_t)) {
  char tmp[CHAT_MAX_MESSAGE_LEN];
  char *buf = tmp;
  size_t len = CHAT_MAX_MESSAGE_LEN;
  client_s *c = fio_udata_get(io);
  fio_stream_read(&c->input, &buf, &len);
  if (!len)
    return -1;
  char *end = memchr(buf, '\n', len);
  if ((!end) | (end == buf))
    return -1;
  len = (size_t)(end - buf) + 1;
  task(io, buf, len);
  fio_stream_advance(&c->input, len);
  return 0;
}

/** for the first input line of the Chat protocol. */
FIO_SFUNC void on_data_login(fio_s *io) {
  if (on_data_read(io))
    return;
  if (on_data_process_line(io, on_data_first_line))
    return;
  fio_protocol_set(io, &CHAT_PROTOCOL_CHAT);
  fio_subscribe(.io = io);
  on_data_chat(io);
}

/** for each subsequent message / line in the Chat protocol. */
FIO_SFUNC void on_data_chat(fio_s *io) {
  if (on_data_read(io))
    return;
  while (!on_data_process_line(io, on_data_message_line))
    ;
}

/** Called when a login process should be performed. */
FIO_SFUNC void on_shutdown(fio_s *io) {
  fio_write(io, "Server shutting down, goodbye...\n", 33);
}

/* *****************************************************************************
Starting the program - main()
***************************************************************************** */

int main(int argc, char const *argv[]) {
  // FIO_NAME_TEST(stl, letter)();
  /* initialize the CLI options */
  fio_cli_start(
      argc,
      argv,
      0, /* allow 1 unnamed argument - the address to connect to */
      1,
      "A simple plain-text chat example, using " FIO_POLL_ENGINE_STR
      " and listening on the "
      "specified URL. i.e.\n"
      "\tNAME <url>\n\n"
      "Unix socket examples:\n"
      "\tNAME unix://./my.sock\n"
      "\tNAME /full/path/to/my.sock\n"
      "\nTCP/IP socket examples:\n"
      "\tNAME tcp://localhost:3000/\n"
      "\tNAME localhost://3000\n",
      FIO_CLI_BOOL("--verbose -V -d print out debugging messages."),
      FIO_CLI_BOOL("--log -v log HTTP messages."),
      FIO_CLI_INT("--workers -w (4) number of worker processes to use."),
      FIO_CLI_PRINT_LINE(
          "NOTE: requests are limited to 32Kb and 16 headers each."));

  /* review CLI for logging */
  if (fio_cli_get_bool("-V")) {
    FIO_LOG_LEVEL = FIO_LOG_LEVEL_DEBUG;
  }
  /* review CLI connection address (in URL format) */
  FIO_ASSERT(!fio_listen(.url = fio_cli_unnamed(0), .on_open = on_open),
             "Could not open listening socket as requested.");
  FIO_LOG_INFO("\n\tStarting plain text Chat server example app."
               "\n\tEngine: " FIO_POLL_ENGINE_STR "\n\tWorkers: %d"
               "\n\tPress ^C to exit.",
               fio_srv_workers(fio_cli_get_i("-w")));
  fio_srv_start(fio_cli_get_i("-w"));
  FIO_LOG_INFO("Shutdown complete.");
  fio_cli_end();
  return 0;
}
