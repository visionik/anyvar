#ifndef ANYVAR_TEST_SUITE_H
#define ANYVAR_TEST_SUITE_H

#include "anyvar.h"
#include "unity.h"

void testScalars_zeroInitIsNull(void);
void testScalars_roundTripNumbers(void);
void testScalars_roundTripFloatingPoint(void);
void testScalars_copyPreservesValues(void);

void testBuffers_borrowedStringDoesNotOwnMemory(void);
void testBuffers_copiedStringIsIndependent(void);
void testBuffers_copiedBinaryIsIndependent(void);
void testBuffers_copyCreatesOwnedStorage(void);

void testContainers_arrayPushGetAndLen(void);
void testContainers_fixedArrayRejectsOverflow(void);
void testContainers_fixedMapBorrowedKeyWorks(void);
void testContainers_mapGrowthKeepsEntries(void);
void testContainers_mapSetGetOverwriteAndLen(void);
void testContainers_customAllocatorHooksAreUsed(void);
void testContainers_copyArrayAndMapValues(void);
void testContainers_malformedContainersAndFixedMapOverflow(void);
void testContainers_publicNullAndTypeErrors(void);

void testErrors_arrayGetOutOfBounds(void);
void testErrors_mapMissingKey(void);
void testErrors_convertToBackendAndBack(void);
void testErrors_allocatorFailurePaths(void);
void testErrors_backendScalarVtableCoversBridgePaths(void);
void testErrors_backendContainerVtableCoversBridgePaths(void);
void testErrors_invalidArgumentsAndTypeMismatches(void);
void testErrors_malformedBackendWrappersReturnSafeDefaults(void);
void testErrors_invalidInternalShapesReturnTypeErrors(void);
void testErrors_zeroLengthBuffersAndBridgeEdgeCases(void);
void testErrors_randomizedRoundTrips(void);

#endif
