import type {
  BlockTemplate,
  PoolChunk,
  PoolCluster,
  PoolTemplates,
  PoolTransaction,
} from './pool-template';

/** The fields of a native block or template tx that the view reads. */
export interface TemplateElement {
  virtual: number;
  fee: number;
  cluster: number;
}

export const BLOCK_VIRTUAL_SIZE = 1_000_000;

// Each cluster of a template is one chunk of the view, its members in template
// order. Cluster ids are local to a template, so they are made global here.
export function toPoolTemplates(
  templates: readonly (readonly TemplateElement[])[],
  first = 0,
  capacity = BLOCK_VIRTUAL_SIZE
): PoolTemplates {
  const transactions: PoolTransaction[] = [];
  const chunks: PoolChunk[] = [];
  const clusters: PoolCluster[] = [];
  const blocks: BlockTemplate[] = [];

  templates.forEach((elements, template) => {
    const local = new Map<number, number>();
    const block: BlockTemplate = { chunks: [], count: elements.length, size: 0, fee: 0 };
    elements.forEach((element, index) => {
      let cluster = local.get(element.cluster);
      if (cluster === undefined) {
        cluster = chunks.length;
        local.set(element.cluster, cluster);
        chunks.push({ cluster, transactions: [], size: 0, fee: 0 });
        clusters.push({ transactions: [], chunks: [cluster] });
        block.chunks.push(cluster);
      }

      const id = transactions.length;
      transactions.push({
        hash: `#${String(index + 1)}`,
        size: element.virtual,
        fee: element.fee,
        parents: [],
        children: [],
        cluster,
        chunk: cluster,
        template,
        position: index + 1,
      });

      chunks[cluster].transactions.push(id);
      chunks[cluster].size += element.virtual;
      chunks[cluster].fee += element.fee;
      clusters[cluster].transactions.push(id);
      block.size += element.virtual;
      block.fee += element.fee;
    });

    blocks.push(block);
  });

  return { transactions, chunks, clusters, templates: blocks, capacity, first };
}
