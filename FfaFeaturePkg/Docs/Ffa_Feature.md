# Firmware Framework for A-Profile (FF-A) Introduction

## Overview

The Firmware Framework for A-Profile (FF-A) is a set of guidelines and best practices for managing firmware in trusted
environments for A-Profile systems. It aims to provide a secure and reliable interface for firmware components, ensuring
that firmware partitions are isolated and protected from each other, thereby enhancing the overall security and stability
of the system.

See [Firmware Framework for A-Profile (FF-A) specification](https://developer.arm.com/documentation/den0077/latest)
for more information on the topic.

## Repository Structure

While the core functionality of FF-A is supported through mainline EDK2 repo, this repository provides additional
features and enhancements specific to the FF-A framework.

The repository is organized to host a variety of components, including:

- Libraries providing bootstrapping and initialization routines for the secure partitions in UEFI style.
- Libraries containing additional FF-A functionalities, such as notification set and get.
- Secure partitions that implement specific FF-A services in UEFI style.
- Secure partitions that implement FF-A services in a Rust environment.

### Software Components

#### Libraries

| Name | Description |
|------|-------------|
| ArmFfaLibEx | Provides FF-A resource information queries and notification management on top of ArmFfaLib. |

### Rust Crates for Services

This section only lists the Rust crates available in _this_ repository. The external crates are not included.

| Name | Description |
|------|-------------|
| TestServiceLibRust | Rust implementation of the test service for secure partitions. |

#### Secure Partitions

| Name | Description |
|------|-------------|
| MsSecurePartitionRust | Rust implementation of a secure partition for the FF-A framework. It currently supports `Inter-partition protocol` defined in [FF-A spec](https://developer.arm.com/documentation/den0077/latest) |

#### Test Application

| Name | Description |
|------|-------------|
| FfaPartitionTest | A test application to cover fundamental secure services described above. |

### Platform Integration

See [Platform Integration](PartitionGuide.md) for more information on integrating FF-A with platform firmware.
