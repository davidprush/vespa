// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.clustercontroller.core.hostinfo;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.Objects;

/**
 * Encapsulation of the usage levels for a particular resource type. The resource type
 * itself is not tracked in this class; this must be done on a higher level.
 */
public class ResourceUsage {
    private final Double usage;

    public ResourceUsage(@JsonProperty("usage") Double usage) {
        this.usage = usage;
    }

    /** Resource usage in [0, 1] */
    public Double getUsage() {
        return usage;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        ResourceUsage that = (ResourceUsage) o;
        return Objects.equals(usage, that.usage);
    }

    @Override
    public int hashCode() {
        return Objects.hash(usage);
    }
}
