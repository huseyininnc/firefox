export const description = `
Tests for validation in createBuffer.
`;

import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/util/util.js';
import {
  kAllBufferUsageBits,
  kBufferSizeAlignment,
  kBufferUsages,
} from '../../../capability_info.js';
import { GPUConst } from '../../../constants.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../gpu_test.js';
import { kMaxSafeMultipleOf8 } from '../../../util/math.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

assert(kBufferSizeAlignment === 4);
g.test('size')
  .desc(
    'Test buffer size alignment is validated to be a multiple of 4 if mappedAtCreation is true.'
  )
  .params(u =>
    u
      .combine('mappedAtCreation', [false, true])
      .beginSubcases()
      .combine('size', [
        0,
        kBufferSizeAlignment * 0.5,
        kBufferSizeAlignment,
        kBufferSizeAlignment * 1.5,
        kBufferSizeAlignment * 2,
      ])
  )
  .fn(t => {
    const { mappedAtCreation, size } = t.params;
    const isValid = !mappedAtCreation || size % kBufferSizeAlignment === 0;
    const usage = BufferUsage.COPY_SRC;

    t.shouldThrow(isValid ? false : 'RangeError', () =>
      t.createBufferTracked({ size, usage, mappedAtCreation })
    );
  });

g.test('limit')
  .desc('Test buffer size is validated against maxBufferSize.')
  .params(u => u.beginSubcases().combine('sizeAddition', [-1, 0, +1]))
  .fn(t => {
    const { sizeAddition } = t.params;
    const size = t.makeLimitVariant('maxBufferSize', { mult: 1, add: sizeAddition });
    const isValid = size <= t.device.limits.maxBufferSize;
    const usage = BufferUsage.COPY_SRC;
    t.expectGPUError('validation', () => t.createBufferTracked({ size, usage }), !isValid);
  });

const kInvalidUsage = 0x8000;
assert((kInvalidUsage & kAllBufferUsageBits) === 0);
g.test('usage')
  .desc('Test combinations of zero to two usage flags are validated to be valid.')
  .params(u =>
    u
      .combine('usage1', [0, ...kBufferUsages, kInvalidUsage])
      .combine('usage2', [0, ...kBufferUsages, kInvalidUsage])
      .beginSubcases()
      .combine('mappedAtCreation', [false, true])
  )
  .fn(t => {
    const { mappedAtCreation, usage1, usage2 } = t.params;
    const usage = usage1 | usage2;

    const isValid =
      usage !== 0 &&
      (usage & ~kAllBufferUsageBits) === 0 &&
      ((usage & GPUBufferUsage.MAP_READ) === 0 ||
        (usage & ~(GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ)) === 0) &&
      ((usage & GPUBufferUsage.MAP_WRITE) === 0 ||
        (usage & ~(GPUBufferUsage.COPY_SRC | GPUBufferUsage.MAP_WRITE)) === 0);

    t.expectGPUError(
      'validation',
      () => t.createBufferTracked({ size: kBufferSizeAlignment * 2, usage, mappedAtCreation }),
      !isValid
    );
  });

const BufferUsage = GPUConst.BufferUsage;

g.test('createBuffer_invalid_and_oom')
  .desc(
    `When creating a mappable buffer, it's expected that shmem may be immediately allocated
(in the content process, before validation occurs in the GPU process). If the buffer is really
large, though, it could fail shmem allocation before validation fails. Ensure that OOM error is
hidden behind the "more severe" validation error.`
  )
  .paramsSubcasesOnly(u =>
    u.combineWithParams([
      { _valid: true, usage: BufferUsage.UNIFORM, size: 16 },
      { _valid: true, usage: BufferUsage.STORAGE, size: 16 },
      // Invalid because UNIFORM is not allowed with map usages.
      { usage: BufferUsage.MAP_WRITE | BufferUsage.UNIFORM, size: 16 },
      { usage: BufferUsage.MAP_WRITE | BufferUsage.UNIFORM, size: kMaxSafeMultipleOf8 },
      { usage: BufferUsage.MAP_WRITE | BufferUsage.UNIFORM, size: 0x20_0000_0000 }, // 128 GiB
      { usage: BufferUsage.MAP_READ | BufferUsage.UNIFORM, size: 16 },
      { usage: BufferUsage.MAP_READ | BufferUsage.UNIFORM, size: kMaxSafeMultipleOf8 },
      { usage: BufferUsage.MAP_READ | BufferUsage.UNIFORM, size: 0x20_0000_0000 }, // 128 GiB
    ] as const)
  )
  .fn(t => {
    const { _valid, usage, size } = t.params;

    t.expectGPUError('validation', () => t.createBufferTracked({ size, usage }), !_valid);
  });
