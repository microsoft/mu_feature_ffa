=================================
Project Mu Feature FFA Repository
=================================

============================= ================= =============== ===================
 Host Type & Toolchain        Build Status      Test Status     Code Coverage
============================= ================= =============== ===================
Windows_VS2022_               |WindowsCiBuild|  |WindowsCiTest| |WindowsCiCoverage|
Ubuntu_GCC5_                  |UbuntuCiBuild|   |UbuntuCiTest|  |UbuntuCiCoverage|
============================= ================= =============== ===================

This repository is part of Project Mu.  Please see Project Mu for details https://microsoft.github.io/mu.

This repository provides additional implementations, features and enhancements to the Firmware Framework for A-Profile
(FF-A) specification. The main framework is supported through EDK2 components.

Detailed Information
====================

The key documents for this repository are:

* `Firmware Framework for A-Profile Feature Package Overview <FfaFeaturePkg/Docs/Ffa_Feature.md>`_
* `Platform Integration Guides <FfaFeaturePkg/Docs/PartitionGuide.md>`_

Repository Philosophy
=====================

Like other Project MU feature repositories, the Project MU FFA feature repo does not strictly follow the EDKII releases,
but instead has a continuous main branch which will periodically receive cherry-picks of needed changes from EDKII. For
stable builds, release tags will be used instead to determine commit hashes at stable points in development. Release
branches may be created as needed to facilitate a specific release with needed features, but this should be avoided.

Code of Conduct
===============

This project has adopted the Microsoft Open Source Code of Conduct https://opensource.microsoft.com/codeofconduct/

For more information see the Code of Conduct FAQ https://opensource.microsoft.com/codeofconduct/faq/
or contact `opencode@microsoft.com <mailto:opencode@microsoft.com>`_. with any additional questions or comments.

Contributions
=============

Contributions are always welcome and encouraged!
Please open any issues in the Project Mu GitHub tracker and read https://microsoft.github.io/mu/How/contributing/


Copyright & License
===================

| Copyright (C) Microsoft Corporation
| SPDX-License-Identifier: BSD-2-Clause-Patent

.. ===================================================================
.. This is a bunch of directives to make the README file more readable
.. ===================================================================

.. CoreCI

.. _Windows_VS2022: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=182&&branchName=main
.. |WindowsCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status%2FCI%2FFeature%20FFA%2FMu%20Feature%20FFA%20-%20CI%20-%20Windows%20VS?repoName=microsoft%2Fmu_feature_ffa&branchName=main
.. |WindowsCiTest| image:: https://img.shields.io/azure-devops/tests/projectmu/mu/182.svg
.. |WindowsCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue

.. _Ubuntu_GCC5: https://dev.azure.com/projectmu/mu/_build/latest?definitionId=183&&branchName=main
.. |UbuntuCiBuild| image:: https://dev.azure.com/projectmu/mu/_apis/build/status%2FCI%2FFeature%20FFA%2FMu%20Feature%20FFA%20-%20CI%20-%20GCC5?repoName=microsoft%2Fmu_feature_ffa&branchName=main
.. |UbuntuCiTest| image:: https://img.shields.io/azure-devops/tests/projectmu/mu/183.svg
.. |UbuntuCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue
