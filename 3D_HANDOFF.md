# Webserv 3D scene handoff

The two models are ready to use. The intro, browser dependencies, and hardware benchmark are still pending. This handoff covers the browser demo. [CODEX_HANDOFF.md](CODEX_HANDOFF.md) covers the older server and connection work.

## What to build

1. Open in a dark room with a server rack and a CRT resting on the floor beside it.
2. Bring up the server’s indicator lights first. After a short delay, turn on the CRT.
3. Clicking the screen moves the camera to a close, straight-on view of it.
4. Reveal the real Webserv tester UI inside the screen. Mouse clicks, typing, scrolling, and sending requests should work as they do on the current site. Keep its `#ff3200` accents and `#141414` background.

## Assets in this checkout

| File | Verified from the installed GLB | What to use |
| --- | --- | --- |
| [server.glb](www/assets/server.glb) | 449 KiB, 6,512 triangles, one mesh primitive, three 8×8 textures. | `Server_Body_Placeable_0` uses the `Placeable` material. Its emissive mask already selects the bright areas. Animate the material’s emissive intensity from zero to light them together. |
| [retro_crt_tv.glb](www/assets/retro_crt_tv.glb) | 3.80 MiB, 3,149 triangles, four mesh primitives, five 1024×1024 textures and one 1024×512 texture. | `Object_7` is the screen with emissive material `.013`. Its map contains baked artwork. `Object_9` is the separate transparent glass with material `.016`. Give the screen a neutral boot material and use the screen mesh as the click target. |

The installed files total 4.24 MiB on disk, include their textures, and require no glTF compression decoder. The CRT textures are already at 1K or smaller. Measure this export before resizing them further.

Keep these credits with the demo and repository.

- [Server by MRowa](https://sketchfab.com/3d-models/server-0516ceb2490949428011679fb8bacb0d), [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). Credit the artist and share any distributed adaptation of the model under the same license.
- [Retro Crt Tv by meipal](https://sketchfab.com/3d-models/retro-crt-tv-d3025abdf2b64882bf60f207b2042a5c), [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Credit the artist and indicate any asset changes.

## Suggested implementation

Use Three.js `WebGLRenderer` with `GLTFLoader` and `Raycaster`. These cover the GLB materials, a shared scene and camera, and picking the screen mesh. Use a pinned version served locally, either as ES modules with their dependencies or as a prebuilt browser bundle. Once those files are present, the demo should run without Node or a CDN on the school machine. [Renderer](https://threejs.org/docs/pages/WebGLRenderer.html) · [GLTFLoader](https://threejs.org/docs/pages/GLTFLoader.html) · [Local modules](https://threejs.org/manual/pages/fundamentals.html)

- Start with a simple floor and walls, a low fill light, and emissive materials for the boot sequence. Use a canvas pixel ratio of 1, with real-time shadows and full-screen bloom disabled for the first measurement.
- Use one initial camera pose and one pose facing the CRT. After the camera reaches the second pose, show a same-origin iframe over the screen opening. Inset and clip it to the inner bezel so the curved glass still reads as a CRT. The iframe gives the existing website normal pointer, keyboard, and scroll behavior. Pause continuous 3D rendering once the camera settles, and redraw on changes such as a resize.
- Prototype at `http://127.0.0.1:8088/intro/` by putting its files under `test/defense/site/intro/`. The current listener already serves that document root. Keep the dashboard at `/` and use it as the iframe destination. This also keeps the existing browser tests that expect the dashboard at `/` working.
- Add the following location to the **8088** server block in [test/test.conf](test/test.conf) when implementing the prototype. The listener’s existing document root is separate from `www/assets`.

  ```nginx
  location /assets {
      methods GET;
      root www/assets;
  }
  ```

  Webserv strips the matched location prefix, so `/assets/server.glb` will open `www/assets/server.glb`. The scene requires no changes to the C++ server or the poll loop.

## Check on the school machine

The target was described as **8 GB RAM and a 10th-generation Core i5**. The GPU, driver, and browser version are unknown, and the scene has not been benchmarked on that machine.

From the repository root, run `make` then `./sr test/test.conf`. The Quick tests dashboard is at `http://127.0.0.1:8088/`, and the manual request builder is at `http://127.0.0.1:8088/manual.html`. If you also run the Intra cases on 8080, adjust the absolute `.bla` CGI path in a local config copy to point to that checkout’s `test/cgi_tester`.

Once the intro exists, test in the actual browser on the machine:

1. Record the browser version, GPU renderer, and viewport size. Chromium’s `chrome://gpu` or Firefox’s `about:support` shows whether WebGL is hardware accelerated.
2. Record one uncached load and a few repeat runs in DevTools. Note how long it takes for both models to load and for the CRT to become clickable.
3. Record the camera transition in the Performance panel. Start with a target of about **33 ms per frame (30 FPS)** on that machine. Check whether the 3D view settles to idle after focusing on the CRT.
4. In the CRT, open the manual tester and send a GET to `/catalog/sample.txt`. Confirm that the click, text fields, scroll, and HTTP response all work. Use the 3D intro by itself for the first benchmark. The native tester’s 100 MB stress run measures a different workload.

Report the measured results before deciding whether to reduce canvas resolution or simplify the CRT glass. The screen transition and working tester come first.
