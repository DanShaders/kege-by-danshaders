#!/usr/bin/python3
import argparse
from collections import namedtuple
import logging as l
from joblib import Parallel, delayed
import pathlib
import re
import subprocess
import threading


ROOT = pathlib.Path(__file__).resolve().parents[1]
DOCKER_REGISTRY = "docker-registry.dshpr.com/"

Container = namedtuple("Container", ["build", "image", "name", "options", "command", "bootstrap"], defaults=([],))

BUILDER = Container(
  build=f"{ROOT}/meta/builder",
  image="kege-by-danshaders/builder",
  name="managed-kege-builder",
  options=[
    "-v", f"{ROOT}:/kege/src",
    "-v", f"{ROOT}/build/var/www/html:/kege/src/ui/build",
    "-v", f"{ROOT}/build/var/run/postgresql:/var/run/postgresql",
    "-v", f"{ROOT}/build/var/run/kege:/var/run/kege",
    "-v", f"{ROOT}/build/bin:/usr/local/bin",
    "-v", f"{ROOT}/build/var/lib/kege/files:/var/lib/kege/files",
    "-v", f"{ROOT}/meta/config.json:/var/lib/kege/config.json",
  ],
  command=["sh", "-c", "trap : TERM INT; tail -f /dev/null & wait"],
  bootstrap=[
    "mkdir -p /kege/build/diff-proto /kege/build/sql-typer /kege/build/debug /kege/build/thread /kege/build/release",
    "cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/tools/diff-proto -B /kege/build/diff-proto",
    "cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/tools/sql-typer -B /kege/build/sql-typer",
    "cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/api -B /kege/build/debug",
    "cmake -G Ninja -DCMAKE_BUILD_TYPE=Thread -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/api -B /kege/build/thread",
    "cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/api -B /kege/build/release",
  ]
)

DB = Container(
  build=f"{ROOT}/meta/postgres",
  image="kege-by-danshaders/postgres",
  name="managed-kege-postgres",
  options=[
    "-v", f"{ROOT}/build/var/lib/postgresql/data:/var/lib/postgresql/data",
    "-v", f"{ROOT}/build/var/run/postgresql:/var/run/postgresql",

    "-e", "POSTGRES_USER=kege",
    "-e", "POSTGRES_PASSWORD=password",
    "-e", "POSTGRES_DB=kege",
  ],
  command=["sh", "-c", "trap : TERM INT; tail -f /dev/null & wait"]
)

NGINX = Container(
  build="dockerhub",
  image="nginx:1.23-alpine",
  name="managed-kege-nginx",
  options=[
    "-v", f"{ROOT}/meta/nginx:/etc/nginx/conf.d",
    "-v", f"{ROOT}/build/var/www/html:/var/www/html/build",
    "-v", f"{ROOT}/build/var/run/kege:/var/run/kege",
    "-v", f"{ROOT}/ui/static:/var/www/html/static",
    "-p", "5001:80"
  ],
  command=["nginx", "-g", "daemon off;"]
)

def expect_one_hash(command):
  process = subprocess.run(command, check=True, capture_output=True)
  if hashes := re.fullmatch(rb"(?:(sha256:)?(?:[0-9a-f]{64})\n)*", process.stdout):
    hashes = process.stdout.strip()
    if not hashes:
      return None

    hashes = list(map(lambda s: (s[7:] if s.startswith(b"sha256:") else s).decode(), hashes.split(b"\n")))
    if len(hashes) > 1:
      l.warning("Found multiple resources satisfying filter. Proceeding with the first match.")
      l.warning(f"Note: While executing {command=}, hashes={list(map(lambda s: s[:12], hashes))}, chosen={hashes[0][:12]}")
    return hashes[0]
  else:
    raise Exception(f"Expected hash list, got ({process.stdout}) (while executing {command=})")


def find_or_build_docker_image(tag, dockerfile):
  find_hash = lambda: expect_one_hash([
    "docker", "images", "--no-trunc", "-qf",
    "reference=" + ("*/" + tag if dockerfile != "dockerhub" else tag)
  ])
  image_hash = find_hash()

  if image_hash is None:
    l.info(f"Image '{tag}' was not found. Building/pulling now...")
    if dockerfile == "dockerhub":
      subprocess.run(["docker", "pull", tag], check=True)
    else:
      subprocess.run(["docker", "build", "-t", f"{DOCKER_REGISTRY}{tag}", dockerfile], check=True)

    image_hash = find_hash()
    if image_hash is None:
      raise Exception(f"Unable to find just-built image")
  else:
    l.info(f"Using image {image_hash[:12]} for {tag}")
  return image_hash


def exec_in(container, command, check=True):
  subprocess.run(["docker", "exec", container, "sh", "-c", command], check=check)


def find_or_create_container(container):
  image_hash = find_or_build_docker_image(container.image, container.build)

  find_hash = lambda: expect_one_hash(["docker", "ps", "--no-trunc", "-aqf", f"ancestor={image_hash}", "-f", f"name={container.name}"])
  container_hash = find_hash()

  if container_hash is None:
    l.info(f"Container '{container.name}' was not found. Creating now...")
    subprocess.run([
      "docker", "run", "-it", "-d",
      *container.options,
      "--name", container.name,
      image_hash,
      *container.command
    ], check=True, capture_output=True)
    container_hash = find_hash()
    if container_hash is None:
      raise Exception(f"Unable to find just-created container")

    # FIXME: Delete container if bootstrapping fails
    for command in container.bootstrap:
      exec_in(container_hash, command)
  else:
    l.info(f"Restarting container {container_hash[:12]}")
    subprocess.run(["docker", "restart", container_hash], check=True, capture_output=True)

  return container_hash


def exec_detached(container, command):
  def inner():
    process = subprocess.run(["docker", "exec", container, *command])
    l.info(f"Exited with code {process.returncode}")

  thr = threading.Thread(target=inner)
  thr.start()
  return thr


def stop_container(container):
  subprocess.run(["docker", "stop", container], capture_output=True, check=True)


def main():
  l.basicConfig(format="%(asctime)s [%(levelname)s] %(message)s", level="INFO")

  parser = argparse.ArgumentParser()
  parser.add_argument('command', choices=["run"])
  options = parser.parse_args()

  match options.command:
    case "run":
      [nginx_hash, db_hash, builder_hash] = Parallel(n_jobs=3, prefer="threads") \
        (delayed(find_or_create_container)(container) for container in [NGINX, DB, BUILDER])

      if not (ROOT / "build" / "bin" / "sql-typer").exists():
        exec_in(builder_hash, "ninja -C /kege/build/sql-typer install")
      if not (ROOT / "build" / "bin" / "diff-proto").exists():
        exec_in(builder_hash, "ninja -C /kege/build/diff-proto install")

      db_thr = exec_detached(db_hash, ["sh", "-c", "docker-entrypoint.sh postgres"])
      api_thr = None
      esbuild_thr = None

      def kill_db():
        nonlocal db_thr
        if db_thr is not None:
          exec_in(db_hash, "pkill -2 postgres -o", check=False)
          db_thr.join(timeout=5)
          while db_thr.is_alive():
            exec_in(db_hash, "pkill -9 postgres", check=False)
            db_thr.join(timeout=2)
          db_thr = None

      def kill_api():
        nonlocal api_thr
        if api_thr is not None:
          exec_in(builder_hash, "pkill -2 KEGE", check=False)
          api_thr.join(timeout=5)
          while api_thr.is_alive():
            exec_in(builder_hash, "pkill -9 ninja KEGE", check=False)
            api_thr.join(timeout=2)
          api_thr = None

      def kill_esbuild():
        nonlocal esbuild_thr
        if esbuild_thr is not None:
          exec_in(builder_hash, "pkill -2 node", check=False)
          esbuild_thr.join()


      while True:
        command = input().split()

        if len(command) and command[0] in {"api", "a", "full-clean"} and len(command) == 1:
          command.append("debug")

        try:
          match command:
            case ["quit" | "q"]:
              break

            case "api" | "a", build_type:
              kill_api()
              api_thr = exec_detached(builder_hash, [
                "sh", "-c",
                f"ninja -C /kege/build/{build_type} && KEGE_ROOT=/var/lib/kege /kege/build/{build_type}/KEGE"
              ])

            case "kill" | "k", "db" | "api" | "esbuild" as what:
              if what == "db":
                kill_db()
              elif what == "api":
                kill_api()
              elif what == "esbuild":
                kill_esbuild()

            case "api-clean" | "ac", build_type:
              kill_api()
              exec_in(builder_hash, f"rm -r /kege/build/{build_type}/* && cmake -G Ninja -DCMAKE_BUILD_TYPE={build_type[0].upper() + build_type[1:]} -DCMAKE_CXX_COMPILER=g++-12 -DCMAKE_C_COMPILER=gcc-12 -S /kege/src/api -B /kege/build/{build_type}")

            case ["ui-node-install" | "ui"]:
              kill_esbuild()
              exec_in(builder_hash, "cd /kege/src/ui && JOBS=`nproc` npm install")

            case ["ui-gen-proto" | "up"]:
              kill_esbuild()
              exec_in(builder_hash, "cd /kege/src/ui && npm run gen-proto")

            case ["ui-watch" | "w"]:
              kill_esbuild()
              esbuild_thr = exec_detached(builder_hash, ["sh", "-c", "cd /kege/src/ui && npm run watch"])

            case _:
              raise Exception("Invalid command")
        except Exception as e:
          l.error("Exception", exc_info=True)

      kill_esbuild()
      kill_api()
      kill_db()

      Parallel(n_jobs=3, prefer="threads") \
        (delayed(stop_container)(container) for container in [nginx_hash, db_hash, builder_hash])


if __name__ == "__main__":
  main()
