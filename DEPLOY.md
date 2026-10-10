# Put the Bank Management System on the internet (no compiling, no VS Code)

You will: (1) put the project on GitHub using only your browser, (2) let a hosting site called **Render** build and run it for you. You get a link like `https://something.onrender.com` that you can share.

> **Not verified by me:** I could not create accounts on GitHub or Render from my environment, so I have **not** deployed this and I do not have a URL for you. The app, the password login and the cloud settings were tested on Linux; the Docker build and the Render steps below follow Render's documented process but are untested. If a step looks different on screen, the Troubleshooting section helps.

## Why GitHub Pages / Netlify / "just upload index.html" will NOT work
Those sites only host files; they cannot run C++. The page would open but show **"Backend offline"**, because every button calls the C++ server. Render can run C++ (inside a Docker container), so the real banking logic and file handling stay exactly as they are.

## Step 1 - Upload the project to GitHub (browser only)
1. Unzip `BankManagementSystem.zip` (right-click -> Extract All).
2. Go to https://github.com , sign up / log in, click **+ -> New repository**. Name: `bank-management-system`. Choose **Public** (or Private) -> **Create repository**.
3. On the new repository page click **"uploading an existing file"**.
4. Open the unzipped folder and select **everything inside it** (Ctrl+A), then drag it into the GitHub page. (Drag the *contents*, not the zip, and not the outer folder.) The files `Dockerfile` and `render.yaml` must end up at the **top level** of the repository, next to `CMakeLists.txt`.
   - Required: `Dockerfile`, `render.yaml`, `CMakeLists.txt`, folders `include`, `src`, `tests`, `frontend`.
   - Optional: `docs`, `README.md`, `DEPLOY.md`, `.gitignore`, `.dockerignore` (hidden files may not show; the app works without them).
   - Do NOT upload `build`, `third_party` or real data files (the zip does not contain them).
   - GitHub's browser upload accepts about 100 files at a time; the project has about 60.
5. Click **Commit changes**.

## Step 2 - Deploy on Render
1. Go to https://render.com and **sign up with GitHub** (allow it to see your repository).
2. Click **New +  ->  Blueprint**, pick your `bank-management-system` repository, and continue. Render reads `render.yaml`.
3. When it asks for **BMS_PASSWORD**, type a password you will share with viewers (the username is `admin`). Click **Apply / Deploy**.
   - If you do not see "Blueprint": use **New + -> Web Service**, choose the repository, **Language: Docker**, **Instance type: Free**, and add environment variables `BMS_USER` = `admin` and `BMS_PASSWORD` = your password. Set **Health Check Path** to `/healthz`.
4. Wait for the build (several minutes: Render compiles the C++ code **and runs all 41 tests**; if a test fails the deploy stops instead of going live broken). Watch the **Logs** tab.
5. When it says **Live**, your URL is shown at the top of the service page (it looks like `https://bank-management-system-xxxx.onrender.com`; Render chooses the exact name). Open it: the browser asks for the login (`admin` + your password), then the dashboard appears with "Backend connected".

Share the URL **and** the password with the people who should see it. Use HTTPS only (Render does this automatically).

## What to expect on the FREE plan (important)
Based on Render's documentation and recent reports (check https://render.com/pricing for current terms):
- The service **goes to sleep after about 15 minutes without visitors**; the next visit takes roughly a minute to wake up.
- The free disk is **temporary**: when the service sleeps, restarts or is redeployed, **all accounts and transactions are erased** and the app starts empty again. That is fine for a demo (create two accounts, deposit, transfer, show the history), but data will **not** persist.
- To keep data permanently you need a **paid** instance with a persistent disk: in `render.yaml` remove `plan: free`, un-comment the `plan: starter` and `disk:` lines (disk mounted at `/data`, which the app already uses), and commit. Other hosts that allow a Docker app plus a storage volume (for example Fly.io or Railway) also work with the same `Dockerfile`; set the same environment variables.

## Which features work where
| Feature | Deployed on Render | Locally on your laptop |
|---|---|---|
| All 11 pages (create, deposit, withdraw, transfer, history, undo, queue, sorted, details, about) | Yes - the same C++ backend and rules | Yes |
| C++ linked list / stack / queue / merge sort, validation | Yes | Yes |
| File handling (`data/*.txt`) | Yes, but the files live inside the server and vanish on the free plan | Yes, permanent in your `data` folder |
| Open and read `data/accounts.txt`, back up with `Copy-Item` | **No** (no access to the server's disk on the free plan) | Yes |
| Password login | Yes (needs `BMS_PASSWORD`) | Optional |
| Run automated tests | Run automatically at every build | `bank_tests.exe` (needs a compiler) |
| Works when the free service is asleep | Wakes up on the next visit (about a minute) | Only while `bank_server` is running |

For your **viva**, demonstrate the data files and restart-persistence on a local run (or on a paid disk); use the live URL to show the interface.

## Security reminder
This is a learning simulation with one shared password. Do not enter real names, real account numbers or real money amounts. Anyone with the URL and password can create and close accounts.

## Troubleshooting
| Problem | What to do |
|---|---|
| Build fails on Render | Open **Logs**, copy the last 30 lines and send them to me |
| "Refusing to listen ... without a password" in the logs | `BMS_PASSWORD` is missing: Service -> **Environment** -> add it -> redeploy |
| Page says "Backend offline" | The service is asleep or restarting; wait a minute and refresh |
| Login box keeps reappearing | wrong password; username is `admin` unless you set `BMS_USER` |
| Data disappeared | Free plan restart (see above) |
| Dockerfile / render.yaml not found | They are not at the top level of the GitHub repository; move them there |

## Environment variables the server understands
`PORT` (set by Render), `BMS_HOST` (set to `0.0.0.0` by the Dockerfile; default `127.0.0.1`), `BMS_DATA_DIR` (default `data`, `/data` in Docker), `BMS_USER` (default `admin`), `BMS_PASSWORD` (required whenever the server listens on a public address). Options `--port`, `--data`, `--frontend` still work and take priority.
