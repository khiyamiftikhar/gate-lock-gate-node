APSTA mode will be used. AP for accessing the device for firmware upgrade. And STA is required for espnow

The firmware provides a  loosely coupled way of component interaction.
Each component can post event to one of the two user created event loops; one for routine events and one for exceptions 
There is a default event loop by the way used by wifi drivers etc, but those events should be handled within the component

The event adapter is a loosely coupled way to add this event  posting feature
The core source file is this free of event loop  details, and another source file kknows this detail
