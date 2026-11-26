import { expect, test } from 'vitest';
import { TemplateSlide, ease } from '@/lib/utils/template-slide';

const TRAVEL = 100;

function slide(animate = true) {
  const loads: string[] = [];
  const subject = new TemplateSlide<string>({
    load: (pool) => {
      loads.push(pool);
    },
    travel: () => TRAVEL,
    animate,
  });
  return { subject, loads };
}

test('template_slide__ease__ends__zero_and_one', () => {
  expect(ease(0, true)).toBe(0);
  expect(ease(1, true)).toBe(1);
  expect(ease(0, false)).toBe(0);
  expect(ease(1, false)).toBe(1);
});

test('template_slide__ease__out_accelerates_in_decelerates', () => {
  expect(ease(0.5, true)).toBe(0.125);
  expect(ease(0.5, false)).toBe(0.875);
});

test('template_slide__set__still__loaded_in_place', () => {
  const { subject, loads } = slide();
  subject.set('a');
  expect(loads).toEqual(['a']);
  expect(subject.moving).toBe(false);
  expect(subject.shift).toBe(0);
});

test('template_slide__move_out__nothing_loaded__no_move', () => {
  const { subject } = slide();
  subject.moveOut(1000);
  expect(subject.moving).toBe(false);
});

test('template_slide__move_out__steps__shift_left_eased', () => {
  const { subject } = slide();
  subject.set('a');
  subject.moveOut(1000);
  expect(subject.moving).toBe(true);
  expect(subject.step(10)).toBe(true);
  expect(subject.shift).toBeCloseTo(0);
  expect(subject.step(510)).toBe(true);
  expect(subject.shift).toBe(-TRAVEL * 0.125);
  expect(subject.step(1010)).toBe(false);
  expect(subject.shift).toBe(-TRAVEL);
});

test('template_slide__set__during_move_out__parked_until_out_then_moves_in', () => {
  const { subject, loads } = slide();
  subject.set('a');
  subject.moveOut(1000);
  subject.step(10);
  subject.set('b');
  expect(loads).toEqual(['a']);
  subject.step(1010);
  expect(loads).toEqual(['a', 'b']);
  expect(subject.shift).toBe(TRAVEL);
  expect(subject.moving).toBe(true);
  subject.step(1020);
  subject.step(1520);
  expect(subject.shift).toBe(TRAVEL * (1 - 0.875));
  expect(subject.step(2020)).toBe(false);
  expect(subject.shift).toBe(0);
});

test('template_slide__set__latest_parked__moves_in', () => {
  const { subject, loads } = slide();
  subject.set('a');
  subject.moveOut(1000);
  subject.step(10);
  subject.set('b');
  subject.set('c');
  subject.step(1010);
  expect(loads).toEqual(['a', 'c']);
});

test('template_slide__set__after_move_out__moves_in', () => {
  const { subject, loads } = slide();
  subject.set('a');
  subject.moveOut(1000);
  subject.step(10);
  subject.step(1010);
  expect(subject.moving).toBe(false);
  subject.set('b');
  expect(loads).toEqual(['a', 'b']);
  expect(subject.shift).toBe(TRAVEL);
  expect(subject.moving).toBe(true);
});

test('template_slide__move_out__twice__one_move', () => {
  const { subject } = slide();
  subject.set('a');
  subject.moveOut(1000);
  subject.step(10);
  subject.step(510);
  subject.moveOut(1000);
  subject.step(1010);
  expect(subject.shift).toBe(-TRAVEL);
});

test('template_slide__not_animated__switches_without_moving', () => {
  const { subject, loads } = slide(false);
  subject.set('a');
  subject.moveOut(1000);
  expect(subject.moving).toBe(false);
  subject.set('b');
  expect(loads).toEqual(['a', 'b']);
  expect(subject.moving).toBe(false);
  expect(subject.shift).toBe(0);
});
