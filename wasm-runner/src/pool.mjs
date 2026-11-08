// A pool of workers that each do one task at a time. A task that overruns its time limit gets its worker
// terminated: that is the only way to stop a synchronous infinite loop. Environment-neutral: the caller supplies
// `spawn`, which wraps a Node worker thread or a browser Worker.
//
//   spawn({ message, error, exit }) -> { post(msg), terminate() }
//     message(m)  the worker answered: m is { ok:true, result } or { ok:false, error }
//     error(e)    the worker crashed          exit()   the worker went away
export class Pool {
  /**
   * @param {{size:number, spawn:Function, maxJobsPerWorker?:number}} o
   * maxJobsPerWorker recycles a worker after that many tasks: WebAssembly memory only ever grows, so a
   * long-lived compiler worker would otherwise hold on to the peak it once needed.
   */
  constructor({ size, spawn, maxJobsPerWorker = Infinity }) {
    this.size = size;
    this.spawnEndpoint = spawn;
    this.maxJobs = maxJobsPerWorker;
    this.workers = new Set();
    this.idle = [];
    this.queue = [];
    this.nextId = 1;
  }

  /** @returns {Promise<{result:any}|{error:string}|{timedOut:true}>} never rejects */
  submit(op, payload, timeoutMs) {
    return new Promise((resolve) => {
      this.queue.push({ op, payload, timeoutMs, resolve });
      this.pump();
    });
  }

  pump() {
    while (this.queue.length) {
      let w = this.idle.pop();
      if (!w && this.workers.size < this.size) w = this.spawn();
      if (!w) return;
      this.dispatch(w, this.queue.shift());
    }
  }

  spawn() {
    const w = { job: null, done: 0, ep: null };
    w.ep = this.spawnEndpoint({
      message: (m) => this.finish(w, m.ok ? { result: m.result } : { error: m.error }),
      error: (e) => this.finish(w, { error: String(e?.stack ?? e?.message ?? e) }, true),
      exit: () => this.finish(w, { error: "worker exited" }, true),
    });
    this.workers.add(w);
    return w;
  }

  dispatch(w, job) {
    const id = this.nextId++;
    w.job = job;
    if (job.timeoutMs) w.timer = setTimeout(() => this.finish(w, { timedOut: true }, true), job.timeoutMs);
    w.ep.post({ id, op: job.op, payload: job.payload });
  }

  finish(w, outcome, kill = false) {
    const job = w.job;
    clearTimeout(w.timer);
    w.job = null;
    if (job) w.done++;
    if (kill || w.done >= this.maxJobs) {
      this.workers.delete(w);
      this.idle = this.idle.filter((x) => x !== w);
      void w.ep.terminate();
    } else if (job) {
      this.idle.push(w);
    }
    if (!job) return;
    job.resolve(outcome);
    this.pump();
  }

  async close() {
    for (const j of this.queue) j.resolve({ error: "closed" });
    this.queue = [];
    await Promise.all([...this.workers].map((w) => w.ep.terminate()));
    this.workers.clear();
    this.idle = [];
  }
}
