/** Bounded concurrency with a bounded wait queue. Rejects instead of growing without limit. */
export class QueueFullError extends Error {
  constructor() {
    super("runner is busy; try again shortly");
  }
}

export class JobQueue {
  private running = 0;
  private waiting: Array<() => void> = [];

  constructor(
    private readonly maxConcurrent: number,
    private readonly maxQueued: number,
  ) {}

  get stats() {
    return { running: this.running, queued: this.waiting.length };
  }

  async run<T>(fn: () => Promise<T>): Promise<T> {
    if (this.running >= this.maxConcurrent) {
      if (this.waiting.length >= this.maxQueued) throw new QueueFullError();
      await new Promise<void>((resolve) => this.waiting.push(resolve));
    } else {
      this.running++;
    }
    try {
      return await fn();
    } finally {
      const next = this.waiting.shift();
      if (next) next(); // hand the slot directly to the next waiter
      else this.running--;
    }
  }
}
