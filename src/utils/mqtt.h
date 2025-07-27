#include <PubSubClient.h>

/// @brief Publishes an MQQT discovery message to register the entity in Home Assistant.
/// @see https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
/// @param client The pointer to the MQTT client.
void publishDiscoveryMessage(PubSubClient& client);

/// @brief Publishes a completion message to notify Home Assistant that washing is complete.
/// @param client The pointer to the MQTT client.
void publishDoneMessage(PubSubClient& client);
