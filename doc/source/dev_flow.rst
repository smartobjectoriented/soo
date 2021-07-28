
.. _dev_flow:

Development  flow
#################

Here is an overview of the *development flow* used in the SOO framework development.

*  **Current**/**latest** version of the framework is available in the ``master``
   branch.
*  The development activities are done in a specific branch. An issue is linked
   with each development branches.
*  Once the development of a new feature has completed, the developer create a ``merge
   request``. The *changes* are reviewed and then merged into the ``master`` branch
   by the *SOO framework maintainers*.

The gitFlow_ figures shows this flow.

Periodically, the *maintainers* publish a major release of the *SOO framework*.
For a new version, the following steps are performed:

*  The main new features has been re-tested
*  The ``CHANGELOG`` file is updated with the new release information (release
   number, description of the main add-on to the framework, ...)
*  A ``tag`` with the version number is created

.. _gitFlow:
.. uml::
   :align: center

   master->>FeatureA: Create branch
   master->>FeatureB: Create branch
   FeatureA->>master: Merge new feature in master
   FeatureB->>master: Merge new feature in master

***********
Development
***********

For the development of new features or improvements, a new branch has to be created.
It should not be any development done directly in the ``master`` branch. Each new
*topic* has to have is own branch. Branches are not reused.

An issue should be linked with each branch (an issue per branch). This issue should
provide:

*  A description of what is addressed
*  (Optional) Information on the advancement of this topic, issues
   found, explanation of the implementation, …

The issue is automatically closed after
the development branch is merged of in the ``master`` branch.

.. note::

   Issue can be (should be) created to document problems, improvement found using
   the framework without having to create a branch.

   The branch can be created when the *issue* is addressed

Create new issue & branch
=========================

To create a branch with an associated issue:

1. Create a new issue. The doc from gitlab: `Create a new
   issue <https://docs.gitlab.com/ee/user/project/issues/managing_issues.html#create-a-new-issue>`__
2. Create the branch from the issue. The doc from gitlab: `Create a new
   branch from an
   issue <https://docs.gitlab.com/ee/user/project/repository/web_editor.html#create-a-new-branch-from-an-issue>`__


Merge in Master
===============

Once the development of a specific topic has completed and been tested, it has to be
*integrated* in the ``master`` branch. It is done by creating a ``merge request``.

By creating a ``merge request``, a developer asks *SOO maintainers* to:

-  Do a review of the modifications
-  Performs the ``merge``

The creation of a ``merge request`` is simple:

1. Form the issue page in gitlab, click ``Create merge request`` button
2. Validate the creation of the ``merge request`` in the new Windows

`Merge request official doc <https://docs.gitlab.com/ee/user/project/merge_requests/creating_merge_requests.html>`__.

*************
Documentation
*************

.. warning:: 

   The Documentation of a new feature **shall** be part of the *feature Merge Request*. 

The issue ``#30 Documentation Update``, with its related branch (``30-documentation-update``), 
can be use for documentation update, improvement. 

The flow is the following:

* Select and update ``30-documentation-update`` branch 
	
.. code-block:: shell

	$ git fetch --all 
	$ git checkout 30-documentation-update
	$ git rebase origin/master

* Update the documentation and push the modification

.. code-block:: shell

	$ git push --force

* Update the issue with info on modification made
* Create a *Merge Request*  


.. note::
	
	Ideally, there is only one commit per changes. If, for some reasons, it is not 
	possible, please inform the *dev team* to lock this branch. 
