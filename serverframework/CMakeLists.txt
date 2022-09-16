file(GLOB CONFIG_SRC config/**.cc)
file(GLOB ENV_SRC env/**.cc)
file(GLOB FIBER_SRC fiber/**.cc)
file(GLOB LOG_SRC log/**.cc)
file(GLOB NET_SRC net/**.cc)
file(GLOB TCP_SRC tcp/**.cc)
file(GLOB UTIL_SRC util/**.cc)

set(LIB_SRC
   ${CONFIG_SRC}
   ${ENV_SRC}
   ${FIBER_SRC}
   ${LOG_SRC}
   ${NET_SRC}
   ${TCP_SRC}
   ${UTIL_SRC} 
    )

add_library(serverframework SHARED ${LIB_SRC})
