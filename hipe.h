#pragma once

// ========================================
//   Please forgive my poor english
// ========================================

/**
 * @brief A steady thread pond
 * It is a steady thread pond that have constant count of threads and is suitable for stable task volume and light task. 
 * It also support task stealing, batch submission, batch execution, thread load balancing. 
*/
#include "./steady_pond.h"

/**
 * @brief A dynamic thread pond. 
 * It is a dynamic thread pond that allow expanding the pond with some threads or delete some threads from the pond.
 * To help you know when to motify the pond, it provide rate view of the threads for supervising.
*/
#include "./dynamic_pond.h"



