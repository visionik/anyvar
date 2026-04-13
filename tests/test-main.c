#include "test-suite.h"

void setUp(void) {}

void tearDown(void) { aVar_resetAllocator(); }

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(testScalars_zeroInitIsNull);
    RUN_TEST(testScalars_roundTripNumbers);
    RUN_TEST(testScalars_roundTripFloatingPoint);
    RUN_TEST(testScalars_copyPreservesValues);

    RUN_TEST(testBuffers_borrowedStringDoesNotOwnMemory);
    RUN_TEST(testBuffers_copiedStringIsIndependent);
    RUN_TEST(testBuffers_copiedBinaryIsIndependent);
    RUN_TEST(testBuffers_copyCreatesOwnedStorage);

    RUN_TEST(testContainers_arrayPushGetAndLen);
    RUN_TEST(testContainers_fixedArrayRejectsOverflow);
    RUN_TEST(testContainers_fixedMapBorrowedKeyWorks);
    RUN_TEST(testContainers_mapGrowthKeepsEntries);
    RUN_TEST(testContainers_mapSetGetOverwriteAndLen);
    RUN_TEST(testContainers_customAllocatorHooksAreUsed);
    RUN_TEST(testContainers_copyArrayAndMapValues);
    RUN_TEST(testContainers_malformedContainersAndFixedMapOverflow);
    RUN_TEST(testContainers_publicNullAndTypeErrors);

    RUN_TEST(testErrors_arrayGetOutOfBounds);
    RUN_TEST(testErrors_mapMissingKey);
    RUN_TEST(testErrors_convertToBackendAndBack);
    RUN_TEST(testErrors_allocatorFailurePaths);
    RUN_TEST(testErrors_backendScalarVtableCoversBridgePaths);
    RUN_TEST(testErrors_backendContainerVtableCoversBridgePaths);
    RUN_TEST(testErrors_invalidArgumentsAndTypeMismatches);
    RUN_TEST(testErrors_malformedBackendWrappersReturnSafeDefaults);
    RUN_TEST(testErrors_invalidInternalShapesReturnTypeErrors);
    RUN_TEST(testErrors_zeroLengthBuffersAndBridgeEdgeCases);
    RUN_TEST(testErrors_randomizedRoundTrips);

    return UNITY_END();
}
