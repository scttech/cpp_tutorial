# The site, built once and served by nginx: no dev server (and no VS Code) needed.
#   docker compose up -d --build        # the site on http://localhost:5173 and the runner on http://localhost:8080
#   docker compose up -d --build web    # after changing code or content: rebuild just the site
# Build context is the repository root (the app reads /content, /wasm-runner and the test header in /runner-image/include).

FROM node:22-bookworm-slim AS build
WORKDIR /repo

# Dependencies first, so a change to the code does not reinstall them.
COPY wasm-runner/package.json wasm-runner/package-lock.json wasm-runner/
RUN cd wasm-runner && npm ci --no-audit --no-fund
COPY app/package.json app/package-lock.json app/
RUN cd app && npm ci --no-audit --no-fund

COPY wasm-runner wasm-runner
COPY content content
COPY runner-image/include runner-image/include
COPY app app

# Where the browser finds the code runner (the runner service publishes this port on the host).
ARG VITE_RUNNER_URL=http://localhost:8080
ENV VITE_RUNNER_URL=$VITE_RUNNER_URL
RUN cd app && npm run build

# Runs as a non-root user and listens on 8080 inside the container.
FROM nginxinc/nginx-unprivileged:1.27-alpine
COPY web/nginx.conf /etc/nginx/conf.d/default.conf
COPY web/headers.inc /etc/nginx/conf.d/headers.inc
COPY --from=build /repo/app/dist /usr/share/nginx/html
EXPOSE 8080
