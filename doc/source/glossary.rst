
########
Glossary
########

.. glossary::

   AgencyUID
      ``AgencyUID`` is a unique 64-bit identifier of the smart object (SOO) consisting in a piece
      of hardware. 
      
   ME
      A ``Mobile Entity`` is a migrating execution environment which contains specific
      applications. The ME is built with the SO3 operating system.

   SPID
      ``Specy ID`` which is a unique 64-bit identifier for a Mobile Entity (ME).
      At a certain time, a smart object can host several instances ME of the same SPID.
      In this case, typically, a synergie should take place and only one instance is
      generally enough to pursue the execution.  
      
   SPAD
      Specy Aptitude Descriptor is a way to identify some functional/behavioural capabilities 
      specific to a ME. 
      The SPAD has a *spadcaps* field to inform about its aptitudes and a valid attribute to
      enable or not the cooperation with other MEs (including MEs of same SPID).
      Example of aptitude is the logic control to monitor, and to manage the temperature in a room.
      Such an aptitude would be defined as *heating* aptitude.
      
      SPAD is not really used at the moment, but it is foreseen to use for ontologies and semantics
      purposes in order to enhance ME logic control.
      
      