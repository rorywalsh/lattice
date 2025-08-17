# Function to apply patches to downloaded dependencies
function(apply_choc_patches)
    set(CHOC_SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/choc-src)
    set(PATCH_DIR ${CMAKE_SOURCE_DIR}/cmake/patches)
    
    # Check if choc source exists
    if(EXISTS ${CHOC_SOURCE_DIR})
        message(STATUS "Applying choc patches for AU plugin compatibility...")
        
        # Apply ObjectiveCHelpers patch for enhanced delegate uniqueness
        set(OBJC_HELPERS_FILE ${CHOC_SOURCE_DIR}/choc/platform/choc_ObjectiveCHelpers.h)
        set(OBJC_HELPERS_PATCH ${PATCH_DIR}/choc_ObjectiveCHelpers.patch)
        
        if(EXISTS ${OBJC_HELPERS_FILE} AND EXISTS ${OBJC_HELPERS_PATCH})
            execute_process(
                COMMAND patch -p0 -N --dry-run
                INPUT_FILE ${OBJC_HELPERS_PATCH}
                WORKING_DIRECTORY ${CHOC_SOURCE_DIR}
                RESULT_VARIABLE PATCH_TEST_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
            )
            
            if(PATCH_TEST_RESULT EQUAL 0)
                execute_process(
                    COMMAND patch -p0 -N
                    INPUT_FILE ${OBJC_HELPERS_PATCH}
                    WORKING_DIRECTORY ${CHOC_SOURCE_DIR}
                    RESULT_VARIABLE PATCH_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET
                )
                
                if(PATCH_RESULT EQUAL 0)
                    message(STATUS "  ✅ Applied choc_ObjectiveCHelpers.h patch for AU delegate uniqueness")
                else()
                    message(WARNING "  ❌ Failed to apply choc_ObjectiveCHelpers.h patch")
                endif()
            else()
                message(STATUS "  ℹ️  choc_ObjectiveCHelpers.h patch already applied or not needed")
            endif()
        endif()
        
        message(STATUS "Choc patches applied successfully")
    else()
        message(WARNING "Choc source directory not found: ${CHOC_SOURCE_DIR}")
    endif()
endfunction()

# Hook to apply patches after choc is downloaded
function(setup_choc_dependency)
    include(FetchContent)
    
    # Your existing choc fetch content setup here
    # Then add:
    FetchContent_GetProperties(choc)
    if(choc_POPULATED)
        apply_choc_patches()
    endif()
endfunction()
