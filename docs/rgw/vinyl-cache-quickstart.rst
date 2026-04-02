VinylCache Quick Start Guide
============================

This guide provides instructions for getting started with RGW VinylCache,
a Varnish-based caching layer for Ceph Object Gateway (RGW).

Overview
--------

VinylCache integrates Varnish Cache with RGW to provide:

- HTTP response caching for GET requests
- Configurable caching policies via VCL (Varnish Configuration Language)
- Cache invalidation support
- Hot configuration updates
- Performance metrics and monitoring

Prerequisites
------------

- Ceph cluster installed and running
- RGW built with VinylCache support (``WITH_RGW_VINYL=ON``)
- Varnish development libraries (optional, for production use)

Installation
------------

1. Build RGW with VinylCache support::

    ./do_cmake.sh -DWITH_RGW_VINYL=ON
    ninja

2. Create the VCL configuration directory::

    sudo mkdir -p /etc/rgw/vinyl

3. Create the default VCL configuration::

    sudo tee /etc/rgw/vinyl/main.vcl << 'EOF'
    vcl 4.1;

    backend default {
        .host = "127.0.0.1";
        .port = "7480";
        .connect_timeout = 3s;
        .first_byte_timeout = 30s;
        .between_bytes_timeout = 60s;
    }

    sub vcl_recv {
        # 允许所有请求通过到 RGW
        return (pass);
    }

    sub vcl_backend_response {
        # 默认缓存行为
        if (beresp.http.Cache-Control ~ "no-store" ||
            beresp.http.Cache-Control ~ "no-cache") {
            set beresp.uncacheable = true;
            return (deliver);
        }

        # 默认 TTL: 1 hour
        set beresp.ttl = 1h;
        set beresp.grace = 30s;

        return (deliver);
    }

    sub vcl_deliver {
        # 添加缓存状态头
        if (obj.hits > 0) {
            set resp.http.X-Cache = "HIT";
        } else {
            set resp.http.X-Cache = "MISS";
        }

        return (deliver);
    }
    EOF

Configuration
-------------

ceph.conf Configuration
~~~~~~~~~~~~~~~~~~~~~~

Add the following configuration options to your ``ceph.conf``::

    [global]
    # VinylCache configuration
    rgw_vinyl_config_dir = /etc/rgw/vinyl
    rgw_vinyl_cache_enabled = true
    rgw_vinyl_http2_enabled = true
    rgw_vinyl_max_connections = 100
    rgw_vinyl_connect_timeout_ms = 3000
    rgw_vinyl_backend_timeout_ms = 30000
    rgw_vinyl_max_object_size = 10485760

Frontend Configuration
~~~~~~~~~~~~~~~~~~~~~~~

Enable the VinylCache frontend::

    [client.rgw.{instance-name}]
    rgw_frontends = vinyl port=7480 vcl_dir=/etc/rgw/vinyl

Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

The VinylCache frontend accepts the following arguments:

- ``port``: Listen port (default: 7480)
- ``vcl_dir``: VCL configuration directory (default: /etc/rgw/vinyl)
- ``vcl_file``: VCL configuration file (default: main.vcl)
- ``cache_enabled``: Enable/disable caching (default: true)
- ``num_threads``: Number of worker threads

Basic Usage
-----------

Start RGW with VinylCache::

    radosgw --rgw-frontends="vinyl port=7480 vcl_dir=/etc/rgw/vinyl"

Or use environment variables::

    RGW_VINYL_PORT=7480 RGW_VINYL_VCL_DIR=/etc/rgw/vinyl \
        radosgw --rgw-frontends="vinyl port=$RGW_VINYL_PORT"

Or use the provided startup script::

    ./src/rgw/rgw_vinyl/start.sh --port 7480 --vcl-dir /etc/rgw/vinyl

Using the Startup Script
~~~~~~~~~~~~~~~~~~~~~~~~

The ``start.sh`` script provides a convenient way to start VinylCache::

    # Basic usage
    ./start.sh --port 7480

    # With custom VCL directory
    ./start.sh --port 7480 --vcl-dir /etc/rgw/vinyl

    # Using environment variables
    RGW_VINYL_PORT=7480 RGW_VINYL_VCL_DIR=/etc/rgw/vinyl ./start.sh

Configuration Options
~~~~~~~~~~~~~~~~~~~~~~

The script supports the following options:

- ``-p, --port``: Listen port (default: 7480)
- ``-v, --vcl-dir``: VCL configuration directory (default: /etc/rgw/vinyl)
- ``-f, --vcl-file``: VCL configuration file (default: main.vcl)
- ``-l, --log-dir``: Log directory (default: /var/log/rgw)
- ``-c, --cache-enabled``: Enable/disable caching (default: true)
- ``-n, --num-threads``: Number of worker threads

Testing
-------

1. Create a test bucket::

    aws s3 mb s3://test-bucket --endpoint-url=http://localhost:7480

2. Upload an object::

    echo "Hello VinylCache" | aws s3 cp - s3://test-bucket/test.txt \
        --endpoint-url=http://localhost:7480

3. Verify caching (first request should be MISS)::

    curl -I http://localhost:7480/test-bucket/test.txt

    # Expected response headers:
    # X-Cache: MISS

4. Verify cache hit (second request should be HIT)::

    curl -I http://localhost:7480/test-bucket/test.txt

    # Expected response headers:
    # X-Cache: HIT

5. Upload a new version of the object::

    echo "Updated content" | aws s3 cp - s3://test-bucket/test.txt \
        --endpoint-url=http://localhost:7480

6. Verify cache miss after update::

    curl -I http://localhost:7480/test-bucket/test.txt

    # Expected response headers:
    # X-Cache: MISS

Advanced Configuration
----------------------

Custom VCL Policies
~~~~~~~~~~~~~~~~~~~~

You can customize caching behavior by modifying the VCL configuration.
The following subroutines are called at different stages of request processing:

- ``vcl_recv``: Called when a request is received. Use to decide whether
  to cache the request.
- ``vcl_backend_response``: Called when the backend response is received.
  Use to modify caching behavior based on response headers.
- ``vcl_deliver``: Called before the response is delivered to the client.
  Use to add custom headers.

Example: Cache Only Specific Content Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To cache only images and static assets::

    sub vcl_backend_response {
        # Check Content-Type header
        if (beresp.http.Content-Type ~ "^image/" ||
            beresp.http.Content-Type ~ "text/css" ||
            beresp.http.Content-Type ~ "application/javascript") {
            set beresp.ttl = 1d;
            return (deliver);
        }

        # Don't cache other content types
        set beresp.uncacheable = true;
        return (deliver);
    }

Example: Cache Based on Query String
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To cache requests with specific query parameters::

    sub vcl_recv {
        # Normalize query string for caching
        if (req.url ~ "(\?|&)cache=true(&|$)") {
            return (hash);
        }
        return (pass);
    }

Hot Configuration Updates
-------------------------

VinylCache supports hot configuration updates without restarting the service.
When the VCL configuration is modified, the changes take effect on the next
request.

To trigger a configuration reload::

    # Edit the VCL configuration
    sudo vim /etc/rgw/vinyl/main.vcl

    # The changes will take effect automatically on the next request

Troubleshooting
---------------

Log Files
~~~~~~~~~

VinylCache logs are written to the RGW log directory::

    /var/log/rgw/vinyl-<port>.log

Common Issues
~~~~~~~~~~~~

**1. Cache not working (always MISS)**

- Check that ``rgw_vinyl_cache_enabled = true`` in ceph.conf
- Verify VCL configuration is valid
- Check that the backend is accessible

**2. Objects not being served**

- Verify the port is not already in use
- Check that the VCL backend configuration is correct
- Review logs for error messages

**3. Performance issues**

- Increase ``rgw_vinyl_max_connections`` for higher throughput
- Adjust TTL values in VCL configuration
- Consider using SSD storage for cache backend

API Reference
-------------

VCL Configuration Options
~~~~~~~~~~~~~~~~~~~~~~~~~~

The following configuration options are available:

+-------------------+---------------+----------------------------------+
| Option            | Default       | Description                      |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_config_ | /etc/rgw/vinyl| VCL configuration directory      |
| dir               |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_cache_  | true          | Enable/disable caching           |
| enabled           |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_http2_  | true          | Enable HTTP/2 support            |
| enabled           |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_max_    | 100           | Maximum concurrent connections   |
| connections       |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_connect_| 3000          | Backend connect timeout (ms)     |
| timeout_ms        |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_backend_| 30000         | Backend request timeout (ms)      |
| timeout_ms        |               |                                  |
+-------------------+---------------+----------------------------------+
| rgw_vinyl_max_    | 10485760      | Maximum cacheable object size    |
| object_size       |               | (bytes)                          |
+-------------------+---------------+----------------------------------+

VCL Built-in Variables
~~~~~~~~~~~~~~~~~~~~~~

VinylCache provides the following VCL variables:

- ``req``: Request object
- ``req.http.*``: Request headers
- ``req.url``: Request URL
- ``req.method``: Request method (GET, PUT, etc.)
- ``beresp``: Backend response object
- ``beresp.http.*``: Backend response headers
- ``beresp.ttl``: Cache TTL
- ``obj``: Cached object (read-only)
- ``obj.hits``: Number of cache hits
- ``resp``: Response to client
- ``resp.http.*``: Response headers

See Also
--------

- `Varnish Documentation <https://www.varnish-software.com/developers/tutorials/>`_
- :doc:`/radosgw/index`
- :doc:`/rados/config-ref`
