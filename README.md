# QuickJS webserver-c

A simple QuickJS HTTP streaming Web server module written in C.

This project is the modified example code that is used for the article:
[Making a simple HTTP webserver in C](https://bruinsslot.jp/post/simple-http-webserver-in-c/).

## Compile with

```bash
$ gcc -L./quickjs -fvisibility=hidden -shared -I ./quickjs -g -ggdb -O -Wall webserver.c -o webserver.so
```

## JavaScript signature
```javascript
#!/usr/bin/env -S ./qjs --std
import {webserver} from './webserver.so';
try {
  webserver('parec -d @DEFAULT_MONITOR@', (status) => {
    console.log(status);
  });
} catch (e) {
  console.log(e);
}
// Reads as long as the pipe is open (until request is aborted)
```

## CLI server usage

```bash
$ ./qjs -m ./webserver.js
```

## Client usage
```javascript
let abortable = new AbortController();
let {signal} = abortable;
let request = await fetch(`http://localhost:8080`, {
                cache: 'no-store',
                signal,
              }).catch(console.error);
let response = request.body;
response.pipeTo(new WritableStream({
  write: async(v) => {
    // Do stuff with stream
    console.log(v.length);
  },
  close() {
    console.log('closed');
  },
  abort(reason) {
    console.log(reason);
  }
})).catch(console.warn);
// Abort the stream
abortable.abort('Done streaming');
```
