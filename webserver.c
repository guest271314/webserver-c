// webserver.c
//
// QuickJS Web server module
// https://github.com/guest271314/webserver-c/tree/quickjs-webserver
// guest271314 3-30-2023
//
// Modified from 
// https://github.com/jpbruinsslot/webserver-c
//
// Compile with 
// gcc -L./quickjs -fvisibility=hidden -shared -I ./quickjs
// -g -ggdb -O -Wall webserver.c -o webserver.so
// 
// JavaScript signature
// webserver(command, callback)
// Reads as long as the pipe is open
//
// CLI server usage
// ./webserver "parec -d @DEFAULT_MONITOR"
//
// Client usage 
// fetch('http://localhost:8080')
//
// Copyright 2023 J.P.H. Bruins Slot
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to
// do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "quickjs.h"
#include "cutils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024

static JSValue module_webserver(JSContext* ctx,
                                JSValueConst this_val,
                                int argc,
                                JSValueConst argv[]) {
  // Check for correct callback function
  if(!JS_IsFunction(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "argument 2 must be a function");
  }
  // man signal.h
  signal(SIGPIPE, SIG_IGN);
  const char* command;
  /* Convert the command to an UTF-8 string */
  command = JS_ToCString(ctx, argv[0]);

  void status(char* str) {
    JSValue callback, params[1];
    params[0] = JS_NewString(ctx, str);
    callback = JS_Call(ctx, argv[1], JS_UNDEFINED, 1, params);
    JS_FreeValue(ctx, callback);
    JS_FreeValue(ctx, params[0]);
  }

  char buffer[BUFFER_SIZE];
  // man 7 tcp
  // man socket
  // man sys_socket.h
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    // man strerror
    // man errno
    return JS_ThrowInternalError(ctx, "server error (socket): %s",
                                 strerror(errno));
  }
  status("socket created successfully");

  // man 7 ip
  struct sockaddr_in host_addr;
  int host_addrlen = sizeof(host_addr);

  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(PORT);
  host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  struct sockaddr_in client_addr;
  int client_addrlen = sizeof(client_addr);

  // man bind
  if (bind(sockfd, (struct sockaddr*)&host_addr, host_addrlen) != 0) {
    return JS_ThrowInternalError(ctx, "webserver (bind): %s", strerror(errno));
  }
  status("socket successfully bound to address");

  // man 2 listen
  if (listen(sockfd, SOMAXCONN) != 0) {
    return JS_ThrowInternalError(ctx, "webserver (listen): %s",
                                 strerror(errno));
  }
  status("server listening for connections");

  for (;;) {
    // man 2 accept
    int request =
        accept(sockfd, (struct sockaddr*)&host_addr, (socklen_t*)&host_addrlen);
    if (request < 0) {
      JS_ThrowInternalError(ctx, "server error (accept): %s", strerror(errno));
      continue;
    }
    status("connection accepted");
    // man getsockname
    int sockn = getsockname(request, (struct sockaddr*)&client_addr,
                            (socklen_t*)&client_addrlen);
    if (sockn < 0) {
      JS_ThrowInternalError(ctx, "server error (getsockname): %s",
                            strerror(errno));
      continue;
    }

    // man 2 read
    int readable = read(request, buffer, BUFFER_SIZE);
    if (readable < 0) {
      JS_ThrowInternalError(ctx, "server error (read): %s", strerror(errno));
      continue;
    }

    // man sscanf
    // man inet_ntoa
    // man ntohs
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);

    status((char*)inet_ntoa(client_addr.sin_addr));
    // TODO: Convert ntohs(client_addr.sin_port) to string; should be PORT
    status(method);
    status(uri);
    status(version);

    // https://developer.chrome.com/blog/private-network-access-preflight/
    // https://wicg.github.io/local-network-access/
    char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Server: webserver-c\r\n"
        "Access-Control-Allow-Headers: cache-control\r\n"
        "Cache-Control: no-store\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-type: application/octet-stream\r\n"
        "Access-Control-Allow-Private-Network: true\r\n\r\n";

    // man 2 write
    // man 3 strlen
    int writer = write(request, response, strlen(response));

    if (writer < 0) {
      return JS_ThrowInternalError(ctx, "server error (write): %s",
                                   strerror(errno));
    }
    // 441 * 4
    // https://www1.cs.columbia.edu/~hgs/audio/44.1.html
    uint8_t writable[1764];

    // man popen
    FILE* pipe = popen(command, "r");

    for (;;) {
      // man fread
      size_t count = fread(writable, 1, sizeof(writable), pipe);
      int stream = write(request, writable, count);

      if (stream < 0) {
        // man pclose
        pclose(pipe);
        break;
      }
    }
    // man 2 close
    close(request);
    // Free command string
    JS_FreeCString(ctx, command);
    break;
  }

  return JS_UNDEFINED;
}

// Declare the JSCFunction requiring 2 arguments
static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("webserver", 2, module_webserver),
};

// Export the function on module initialization
static int module_init(JSContext* ctx, JSModuleDef* m) {
  // Sets the export list to the function declarations above
  JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
  return 0;
}

// Shared-object entry point. when loaded by QuickJS will get executed
// (make the function the only exported name of the shared-object)
__attribute__((visibility("default"))) JSModuleDef* js_init_module(
    JSContext* ctx,
    const char* module_name) {
  JSModuleDef* m;
  // Creates a new module, named according to the 'from' string given in the
  // import directive and with the initialization function above.
  m = JS_NewCModule(ctx, module_name, module_init);
  if (!m)
    return NULL;
  // Adds the exports
  JS_AddModuleExportList(ctx, m, module_funcs, countof(module_funcs));
  return m;
}
