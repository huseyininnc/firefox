export const description = `
Validation tests for resolveQuerySet.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUConst } from '../../../../constants.js';
import { kResourceStates, AllFeaturesMaxLimitsGPUTest } from '../../../../gpu_test.js';
import * as vtu from '../../validation_test_utils.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);

export const kQueryCount = 2;

g.test('queryset_and_destination_buffer_state')
  .desc(
    `
Tests that resolve query set must be with valid query set and destination buffer.
- {invalid, destroyed} GPUQuerySet results in validation error.
- {invalid, destroyed} destination buffer results in validation error.
  `
  )
  .params(u =>
    u //
      .combine('querySetState', kResourceStates)
      .combine('destinationState', kResourceStates)
  )
  .fn(t => {
    const { querySetState, destinationState } = t.params;

    const shouldBeValid = querySetState !== 'invalid' && destinationState !== 'invalid';
    const shouldSubmitSuccess = querySetState === 'valid' && destinationState === 'valid';

    const querySet = vtu.createQuerySetWithState(t, querySetState);

    const destination = vtu.createBufferWithState(t, destinationState, {
      size: kQueryCount * 8,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, 0, 1, destination, 0);
    encoder.validateFinishAndSubmit(shouldBeValid, shouldSubmitSuccess);
  });

g.test('first_query_and_query_count')
  .desc(
    `
Tests that resolve query set with invalid firstQuery and queryCount:
- firstQuery and/or queryCount out of range
  `
  )
  .paramsSubcasesOnly([
    { firstQuery: 0, queryCount: kQueryCount }, // control case
    { firstQuery: 0, queryCount: kQueryCount + 1 },
    { firstQuery: 1, queryCount: kQueryCount },
    { firstQuery: kQueryCount, queryCount: 1 },
  ])
  .fn(t => {
    const { firstQuery, queryCount } = t.params;

    const querySet = t.createQuerySetTracked({ type: 'occlusion', count: kQueryCount });
    const destination = t.createBufferTracked({
      size: kQueryCount * 8,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, firstQuery, queryCount, destination, 0);
    encoder.validateFinish(firstQuery + queryCount <= kQueryCount);
  });

g.test('destination_buffer_usage')
  .desc(
    `
Tests that resolve query set with invalid destinationBuffer:
- Buffer usage {with, without} QUERY_RESOLVE
  `
  )
  .paramsSubcasesOnly(u =>
    u //
      .combine('bufferUsage', [
        GPUConst.BufferUsage.STORAGE,
        GPUConst.BufferUsage.QUERY_RESOLVE, // control case
      ] as const)
  )
  .fn(t => {
    const querySet = t.createQuerySetTracked({ type: 'occlusion', count: kQueryCount });
    const destination = t.createBufferTracked({
      size: kQueryCount * 8,
      usage: t.params.bufferUsage,
    });

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    encoder.validateFinish(t.params.bufferUsage === GPUConst.BufferUsage.QUERY_RESOLVE);
  });

g.test('destination_offset_alignment')
  .desc(
    `
Tests that resolve query set with invalid destinationOffset:
- destinationOffset is not a multiple of 256
  `
  )
  .paramsSubcasesOnly(u => u.combine('destinationOffset', [0, 128, 256, 384]))
  .fn(t => {
    const { destinationOffset } = t.params;
    const querySet = t.createQuerySetTracked({ type: 'occlusion', count: kQueryCount });
    const destination = t.createBufferTracked({
      size: 512,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, 0, kQueryCount, destination, destinationOffset);
    encoder.validateFinish(destinationOffset % 256 === 0);
  });

g.test('resolve_buffer_oob')
  .desc(
    `
Tests that resolve query set with the size oob:
- The size of destinationBuffer - destinationOffset < queryCount * 8
  `
  )
  .paramsSubcasesOnly(u =>
    u.combineWithParams([
      { queryCount: 2, bufferSize: 16, destinationOffset: 0, _success: true },
      { queryCount: 3, bufferSize: 16, destinationOffset: 0, _success: false },
      { queryCount: 2, bufferSize: 16, destinationOffset: 256, _success: false },
      { queryCount: 2, bufferSize: 272, destinationOffset: 256, _success: true },
      { queryCount: 2, bufferSize: 264, destinationOffset: 256, _success: false },
    ])
  )
  .fn(t => {
    const { queryCount, bufferSize, destinationOffset, _success } = t.params;
    const querySet = t.createQuerySetTracked({ type: 'occlusion', count: queryCount });
    const destination = t.createBufferTracked({
      size: bufferSize,
      usage: GPUBufferUsage.QUERY_RESOLVE,
    });

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, 0, queryCount, destination, destinationOffset);
    encoder.validateFinish(_success);
  });

g.test('query_set_buffer,device_mismatch')
  .desc(
    'Tests resolveQuerySet cannot be called with a query set or destination buffer created from another device'
  )
  .paramsSubcasesOnly([
    { querySetMismatched: false, bufferMismatched: false }, // control case
    { querySetMismatched: true, bufferMismatched: false },
    { querySetMismatched: false, bufferMismatched: true },
  ] as const)
  .beforeAllSubcases(t => t.usesMismatchedDevice())
  .fn(t => {
    const { querySetMismatched, bufferMismatched } = t.params;

    const kQueryCount = 1;

    const querySetDevice = querySetMismatched ? t.mismatchedDevice : t.device;
    const querySet = t.trackForCleanup(
      querySetDevice.createQuerySet({
        type: 'occlusion',
        count: kQueryCount,
      })
    );

    const bufferDevice = bufferMismatched ? t.mismatchedDevice : t.device;
    const buffer = t.trackForCleanup(
      bufferDevice.createBuffer({
        size: kQueryCount * 8,
        usage: GPUBufferUsage.QUERY_RESOLVE,
      })
    );

    const encoder = t.createEncoder('non-pass');
    encoder.encoder.resolveQuerySet(querySet, 0, kQueryCount, buffer, 0);
    encoder.validateFinish(!(querySetMismatched || bufferMismatched));
  });
