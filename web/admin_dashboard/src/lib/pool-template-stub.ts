import { BLOCK_VIRTUAL_SIZE } from './pool-rates';
import type {
  BlockTemplate,
  PoolChunk,
  PoolCluster,
  PoolTemplates,
  PoolTransaction,
} from './pool-template';

// Placeholder pool until the server publishes its templates. The pool is
// generated from a seed, so a seed always gives the same pool.

type Shape = 'single' | 'chain' | 'child-pays' | 'fan-out' | 'fan-in';

function generator(seed: number): () => number {
  let state = seed | 0;
  return () => {
    state = (state + 0x6d2b79f5) | 0;
    let value = Math.imul(state ^ (state >>> 15), 1 | state);
    value = (value + Math.imul(value ^ (value >>> 7), 61 | value)) ^ value;
    return ((value ^ (value >>> 14)) >>> 0) / 4_294_967_296;
  };
}

type Linearizable = Pick<PoolTransaction, 'size' | 'fee' | 'parents'>;

// Repeatedly take the highest-rate ancestor set of the members not yet taken.
// Members must be listed parents first.
export function linearize(transactions: readonly Linearizable[], members: readonly number[]): number[] {
  const remaining = new Set(members);
  const order: number[] = [];
  const ancestors = (member: number): number[] => {
    const found = new Set<number>();
    const pending = [member];
    while (pending.length > 0) {
      const next = pending.pop() as number;
      if (found.has(next) || !remaining.has(next)) continue;
      found.add(next);
      pending.push(...transactions[next].parents);
    }
    return [...found].sort((left, right) => members.indexOf(left) - members.indexOf(right));
  };

  while (remaining.size > 0) {
    let best: number[] = [];
    let bestRate = -1;
    for (const member of members) {
      if (!remaining.has(member)) continue;
      const set = ancestors(member);
      const fee = set.reduce((total, index) => total + transactions[index].fee, 0);
      const size = set.reduce((total, index) => total + transactions[index].size, 0);
      if (fee / size > bestRate) {
        bestRate = fee / size;
        best = set;
      }
    }

    for (const index of best) {
      remaining.delete(index);
      order.push(index);
    }
  }

  return order;
}

// Merge each transaction into the chunk before it while it pays a higher rate,
// so chunk rates never rise along the linearization.
export function chunk(
  transactions: readonly Linearizable[],
  order: readonly number[]
): Array<Pick<PoolChunk, 'transactions' | 'size' | 'fee'>> {
  const chunks: Array<Pick<PoolChunk, 'transactions' | 'size' | 'fee'>> = [];
  for (const index of order) {
    chunks.push({ transactions: [index], size: transactions[index].size, fee: transactions[index].fee });
    while (chunks.length > 1) {
      const last = chunks[chunks.length - 1];
      const prior = chunks[chunks.length - 2];
      if (last.fee * prior.size <= prior.fee * last.size) break;
      prior.transactions.push(...last.transactions);
      prior.size += last.size;
      prior.fee += last.fee;
      chunks.pop();
    }
  }

  return chunks;
}

// Take chunks by descending rate, starting a new template when the next chunk
// does not fit. Equal rates keep chunk order, which keeps each cluster's
// chunks in mining order.
export function pack(chunks: readonly PoolChunk[], capacity: number): BlockTemplate[] {
  const order = chunks.map((_, index) => index);
  order.sort((left, right) => {
    const a = chunks[left];
    const b = chunks[right];
    return b.fee * a.size - a.fee * b.size || left - right;
  });

  const templates: BlockTemplate[] = [];
  let current: BlockTemplate | undefined;
  for (const index of order) {
    const next = chunks[index];
    if (!current || current.size + next.size > capacity) {
      current = { chunks: [], count: 0, size: 0, fee: 0 };
      templates.push(current);
    }

    current.chunks.push(index);
    current.count += next.transactions.length;
    current.size += next.size;
    current.fee += next.fee;
  }

  return templates;
}

export function stubPoolTemplates(
  seed = 7,
  poolSize = 3_400_000,
  capacity = BLOCK_VIRTUAL_SIZE
): PoolTemplates {
  const random = generator(seed);
  const transactions: PoolTransaction[] = [];
  const clusters: PoolCluster[] = [];
  const hash = () =>
    Array.from({ length: 64 }, () => Math.floor(random() * 16).toString(16)).join('');
  const size = () => {
    const base = Math.round(110 + random() ** 3 * 1_900);
    return random() < 0.01 ? base + Math.round(5_000 + random() * 30_000) : base;
  };
  const rate = () => Math.max(1, Math.round(Math.exp(random() ** 1.7 * Math.log(300)) * 10) / 10);
  const link = (parent: number, child: number) => {
    transactions[parent].children.push(child);
    transactions[child].parents.push(parent);
  };

  let total = 0;
  while (total < poolSize) {
    const draw = random();
    let shape: Shape = 'single';
    let count = 1;
    if (draw >= 0.97) [shape, count] = ['fan-in', 3 + Math.floor(random() * 4)];
    else if (draw >= 0.93) [shape, count] = ['fan-out', 3 + Math.floor(random() * 5)];
    else if (draw >= 0.86) [shape, count] = ['chain', 3 + Math.floor(random() * 3)];
    else if (draw >= 0.74) [shape, count] = [random() < 0.5 ? 'child-pays' : 'chain', 2];

    const cluster = clusters.length;
    const members: number[] = [];
    for (let member = 0; member < count; member += 1) {
      const bytes = size();
      const paid =
        shape === 'child-pays'
          ? member === 0
            ? 1 + Math.round(random() * 10) / 10
            : 40 + Math.round(random() * 800) / 10
          : rate();
      members.push(transactions.length);
      transactions.push({
        hash: hash(),
        size: bytes,
        fee: Math.round(bytes * paid),
        parents: [],
        children: [],
        cluster,
        chunk: -1,
        template: -1,
        position: 0,
      });
      total += bytes;
    }

    if (shape === 'chain' || shape === 'child-pays')
      for (let member = 1; member < count; member += 1) link(members[member - 1], members[member]);
    else if (shape === 'fan-out') {
      for (let member = 1; member < count; member += 1) link(members[0], members[member]);
      if (count > 4) link(members[1], members[count - 1]);
    } else if (shape === 'fan-in')
      for (let member = 0; member < count - 1; member += 1) link(members[member], members[count - 1]);

    clusters.push({ transactions: members, chunks: [] });
  }

  const chunks: PoolChunk[] = [];
  clusters.forEach((cluster, index) => {
    for (const part of chunk(transactions, linearize(transactions, cluster.transactions))) {
      cluster.chunks.push(chunks.length);
      for (const member of part.transactions) transactions[member].chunk = chunks.length;
      chunks.push({ cluster: index, ...part });
    }
  });

  const templates = pack(chunks, capacity);
  templates.forEach((template, index) => {
    let position = 0;
    for (const part of template.chunks) {
      for (const member of chunks[part].transactions) {
        position += 1;
        transactions[member].template = index;
        transactions[member].position = position;
      }
    }
  });

  return { transactions, chunks, clusters, templates, capacity };
}
