# TPM Service

## Overview

The TPM service found in FfaFeaturePkg/Library/TpmService is a reference implementation
of the TPM Service Command Response Buffer Interface Over FF-A specification released by
ARM. See [TPM Service Command Response Buffer Interface Over FF-A](https://developer.arm.com/documentation/den0138/0100/?lang=en)
for more details. The specification is meant to describe one approach to TPM integration
on ARM systems, namely firmware-based TPM integration.

## Communication

The TPM service is a way for the normal-world to communicate with a TPM which should only
be accessible via the secure-world (Arm TrustZone). Through the normal mechanism of FF-A
Direct Request messages the normal world can issue the ABIs specified in the CRB over FF-A
specification to communicate with the TPM.

Communication occurs through a CRB interface which is spec defined via Trusted Computing
Group (TCG) PC Client Specific Platform TPM Profile for TPM2.0. See
[TCG PC Client Platform TPM Profile Specification for TPM 2.0](https://trustedcomputinggroup.org/wp-content/uploads/PC-Client-Specific-Platform-TPM-Profile-for-TPM-2p0-Version-1p06_pub.pdf)
for more details.

The TPM Service State Translation Library found in FfaFeaturePkg/Library/TpmServiceStateTranslationLib
is used to translate the CRB interface into the FIFO interface our QEMU SBSA platform uses
to communicate with the software TPM. See [QEMU TPM Device](https://www.qemu.org/docs/master/specs/tpm.html)
for more details. In short, the TPM is emulated using an external TPM emulator called "swtpm".
This "swtpm" behaves like a hardware TPM and must be initialized/started before attempting to
access it through the TPM emulator in QEMU. Note that building and running QEMU SBSA with
BLD_*_TPM2_ENABLE=TRUE the "swtpm" is automatically initialized and started. If interested,
code for this can be found in QemuRunner.py which is located in Platforms/QemuSbsaPkg/Plugins/QemuRunner
in mu_tiano_platforms.

The TPM service's CRB is accessible by both the normal-world and secure-world but is completely
managed by the TPM service. Upon initialization of the service the CRB will be initialized. After
the completion of every TPM command, the CRB will be cleaned such that it retains a consistent
state. The idea is that the TPM service's CRB mimics that of a normal MMIO CRB in that all
communication goes through the CRB but instead of acting immediately upon adjustments to register
contents, the TPM service needs to be invoked via the TPM service's commands/ABIs.

## Commands

Only a subset of the commands defined in the CRB over FF-A spec are implemented in the reference
implementation. Many of the ABIs that are defined in the CRB over FF-A spec are related to
notifications which are unused in our reference platform. We have a notification service
which we would use in place of these ABIs. More information about the notification service
can be found in the Firmware Framework Arm A-Profile (FF-A) specification v1.3 section 18.7
Inter-partition Setup Protocol.

### Get Interface Version

The Get Interface Version ABI returns the version the reference implementation is based on.
The current version reported is v1.0. This is used to distinguish updates to the CRB over FF-A
specification.

### Start

The Start ABI is used to transmit locality requests and TPM commands to the TPM. These
commands are transferred via the CRB to the platform's communication of choice. (PTP CRB,
Mobile CRB, FIFO, TIS) The Start ABI contains a state machine that manages the current
state the TPM is in: Idle, Ready, and Complete. It only transitions to another state upon
successful transmission of the corresponding TPM command. A diagram of the TPM state for
the CRB interface can be found on page 118 of the TCG2 PC Client Spec. (Figure 4)

Locality requests manage which locality is currently the active locality. (i.e. which
locality is currently in use) This ABI has the capability to either request or relinquish
a locality. Note that to request a differently locality the active locality must first be
relinquished.

Note that while writing to a different locality is not prevented, no actions will take
place unless the locality is first requested. Any state information written to that
locality will be ignored as well as lost once that locality is requested.

Reminder that all CRB state information for the active locality will be cleaned after
every invocation of the Start ABI. This is to maintain a consistent and clean internal
state.

### Manage Locality

The Manage Locality ABI has yet to be officially added to the CRB over FF-A specification,
however, there are plans to add it in the future. This ABI is listed under the
IMPLEMENTATION DEFINED section of Function IDs (0x1fxx_xxxx). The availability of the CRB
localities (i.e. whether a locality is opened or closed) is not something that the TPM
service manages. This information is controlled by TF-A at S-EL3. This ABI is used
exclusively by TF-A to inform the TPM service of the availability of each locality. This
ABI has the capability to open and close any locality.
