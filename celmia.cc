/* 
 * Simulation NS-3 du Réseau WAN Éducatif de Yaoundé
 * Projet: Plateforme Cloud Centralisée pour les Écoles
 * Version: NS-3.46 compatible
 * 
 * Description: Simulation d'un réseau WAN interconnectant plusieurs écoles
 * de Yaoundé (Nkolbisson, Mvog-Ada, Essos, Mendong, Ngoa-Ekellé) 
 * à un serveur cloud central via différentes technologies d'accès.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("YaoundeEducationNetwork");

int main(int argc, char *argv[]) {
    // Paramètres configurables
    bool verbose = true;
    uint32_t nSchools = 5;  // Nombre d'écoles
    uint32_t nStudentsPerSchool = 50;
    double simulationTime = 60.0;  // secondes
    bool enablePcap = true;
    bool enableFlowMonitor = true;
    std::string animFile = "yaounde-education-network.xml";
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("nSchools", "Nombre d'écoles", nSchools);
    cmd.AddValue("nStudents", "Nombre d'étudiants par école", nStudentsPerSchool);
    cmd.AddValue("simTime", "Temps de simulation (s)", simulationTime);
    cmd.AddValue("verbose", "Mode verbose", verbose);
    cmd.AddValue("pcap", "Activer PCAP", enablePcap);
    cmd.AddValue("flowmon", "Activer FlowMonitor", enableFlowMonitor);
    cmd.Parse(argc, argv);
    
    if (verbose) {
        LogComponentEnable("YaoundeEducationNetwork", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
    
    NS_LOG_INFO("=== Simulation du Réseau Éducatif de Yaoundé ===");
    NS_LOG_INFO("Nombre d'écoles: " << nSchools);
    NS_LOG_INFO("Durée de simulation: " << simulationTime << "s");
    
    // ========== CRÉATION DES NŒUDS ==========
    
    // Serveur Cloud Central
    NodeContainer cloudServer;
    cloudServer.Create(1);
    
    // Routeur WAN Central (point d'accès principal)
    NodeContainer wanRouter;
    wanRouter.Create(1);
    
    // Écoles avec leurs routeurs d'accès
    NodeContainer schoolRouters;
    schoolRouters.Create(nSchools);
    
    // Conteneurs pour les dispositifs utilisateurs (ordinateurs, tablettes)
    std::vector<NodeContainer> schoolDevices(nSchools);
    for (uint32_t i = 0; i < nSchools; i++) {
        schoolDevices[i].Create(nStudentsPerSchool);
    }
    
    // ========== CONFIGURATION DU RÉSEAU CŒUR (CLOUD - WAN ROUTER) ==========
    
    PointToPointHelper p2pCore;
    p2pCore.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pCore.SetChannelAttribute("Delay", StringValue("2ms"));
    
    NetDeviceContainer coreDevices;
    coreDevices = p2pCore.Install(cloudServer.Get(0), wanRouter.Get(0));
    
    // ========== CONFIGURATION DES LIAISONS WAN (Écoles - WAN Router) ==========
    
    std::vector<NetDeviceContainer> wanLinks(nSchools);
    std::vector<std::string> schoolNames = {
        "Nkolbisson", "Mvog-Ada", "Essos", "Mendong", "Ngoa-Ekellé"
    };
    
    // Technologies d'accès variées pour chaque école
    std::vector<std::string> accessTech = {
        "Fibre Optique", "4G/5G", "Satellite", "Liaison Radio", "ADSL"
    };
    
    for (uint32_t i = 0; i < nSchools; i++) {
        PointToPointHelper p2pWan;
        
        // Configuration selon la technologie d'accès
        if (i == 0) {  // Fibre optique
            p2pWan.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
            p2pWan.SetChannelAttribute("Delay", StringValue("5ms"));
        } else if (i == 1) {  // 4G/5G
            p2pWan.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
            p2pWan.SetChannelAttribute("Delay", StringValue("20ms"));
        } else if (i == 2) {  // Satellite
            p2pWan.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
            p2pWan.SetChannelAttribute("Delay", StringValue("600ms"));
        } else if (i == 3) {  // Liaison Radio
            p2pWan.SetDeviceAttribute("DataRate", StringValue("200Mbps"));
            p2pWan.SetChannelAttribute("Delay", StringValue("10ms"));
        } else {  // ADSL
            p2pWan.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
            p2pWan.SetChannelAttribute("Delay", StringValue("30ms"));
        }
        
        wanLinks[i] = p2pWan.Install(wanRouter.Get(0), schoolRouters.Get(i));
        
        NS_LOG_INFO("École " << schoolNames[i % schoolNames.size()] 
                    << " - Technologie: " << accessTech[i % accessTech.size()]);
    }
    
    // ========== CONFIGURATION DES RÉSEAUX LOCAUX (LAN) DES ÉCOLES ==========
    
    std::vector<NetDeviceContainer> lanDevices(nSchools);
    
    for (uint32_t i = 0; i < nSchools; i++) {
        // Configuration CSMA pour le LAN de l'école
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
        
        // Connecter le routeur de l'école aux dispositifs
        NodeContainer lanNodes;
        lanNodes.Add(schoolRouters.Get(i));
        lanNodes.Add(schoolDevices[i]);
        
        lanDevices[i] = csma.Install(lanNodes);
    }
    
    // ========== INSTALLATION DE LA PILE INTERNET ==========
    
    InternetStackHelper stack;
    stack.Install(cloudServer);
    stack.Install(wanRouter);
    stack.Install(schoolRouters);
    
    for (uint32_t i = 0; i < nSchools; i++) {
        stack.Install(schoolDevices[i]);
    }
    
    // ========== ATTRIBUTION DES ADRESSES IP ==========
    
    Ipv4AddressHelper address;
    
    // Réseau Core (Cloud - WAN Router)
    address.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer coreInterfaces = address.Assign(coreDevices);
    
    // Réseaux WAN (WAN Router - Écoles)
    std::vector<Ipv4InterfaceContainer> wanInterfaces(nSchools);
    for (uint32_t i = 0; i < nSchools; i++) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.252");
        wanInterfaces[i] = address.Assign(wanLinks[i]);
    }
    
    // Réseaux LAN des écoles
    std::vector<Ipv4InterfaceContainer> lanInterfaces(nSchools);
    for (uint32_t i = 0; i < nSchools; i++) {
        std::ostringstream subnet;
        subnet << "192.168." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        lanInterfaces[i] = address.Assign(lanDevices[i]);
    }
    
    // Configuration du routage
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // ========== CONFIGURATION DES APPLICATIONS ==========
    
    // Serveur sur le Cloud (port 9)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(cloudServer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));
    
    // Clients dans chaque école
    for (uint32_t i = 0; i < nSchools; i++) {
        // Plusieurs clients par école simulant différentes fonctionnalités
        for (uint32_t j = 0; j < 5; j++) {  // 5 clients actifs par école
            UdpEchoClientHelper echoClient(coreInterfaces.GetAddress(0), 9);
            echoClient.SetAttribute("MaxPackets", UintegerValue(100));
            echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0 + i * 0.1)));
            echoClient.SetAttribute("PacketSize", UintegerValue(1024));
            
            uint32_t deviceIdx = j % nStudentsPerSchool;
            ApplicationContainer clientApps = echoClient.Install(schoolDevices[i].Get(deviceIdx));
            clientApps.Start(Seconds(2.0 + i * 0.5 + j * 0.1));
            clientApps.Stop(Seconds(simulationTime));
        }
    }
    
    // ========== CONFIGURATION DES TRACES ET MONITORING ==========
    
    if (enablePcap) {
        p2pCore.EnablePcapAll("yaounde-core");
        // Activer PCAP sélectivement pour éviter trop de fichiers
        for (uint32_t i = 0; i < std::min(nSchools, 2u); i++) {
            std::ostringstream oss;
            oss << "yaounde-school-" << i;
            CsmaHelper csma;
            csma.EnablePcap(oss.str(), lanDevices[i].Get(0), true);
        }
    }
    
    // ========== CONFIGURATION NETANIM POUR VISUALISATION ==========
    AnimationInterface anim(animFile);
    
    // Positionnement des nœuds pour l'animation
    anim.SetConstantPosition(cloudServer.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(wanRouter.Get(0), 50.0, 30.0);
    
    double angle = 0.0;
    double radius = 20.0;
    for (uint32_t i = 0; i < nSchools; i++) {
        double x = 50.0 + radius * std::cos(angle);
        double y = 30.0 + radius * std::sin(angle);
        anim.SetConstantPosition(schoolRouters.Get(i), x, y);
        
        // Position de TOUS les dispositifs autour du routeur de l'école
        for (uint32_t j = 0; j < nStudentsPerSchool; j++) {
            // Disposition en grille compacte autour du routeur
            uint32_t devicesPerRing = 20;  // 20 dispositifs par anneau
            uint32_t ring = j / devicesPerRing;  // Numéro d'anneau
            uint32_t posInRing = j % devicesPerRing;  // Position dans l'anneau
            
            double deviceRadius = 5.0 + (ring * 3.0);  // Rayon croissant par anneau
            double deviceAngle = 2 * M_PI * posInRing / devicesPerRing;
            double dx = x + deviceRadius * std::cos(deviceAngle);
            double dy = y + deviceRadius * std::sin(deviceAngle);
            anim.SetConstantPosition(schoolDevices[i].Get(j), dx, dy);
        }
        
        angle += 2 * M_PI / nSchools;
    }
    
    // Descriptions des nœuds pour NetAnim
    anim.UpdateNodeDescription(cloudServer.Get(0), "Cloud Server");
    anim.UpdateNodeDescription(wanRouter.Get(0), "WAN Router");
    for (uint32_t i = 0; i < nSchools; i++) {
        std::ostringstream desc;
        desc << schoolNames[i % schoolNames.size()] << " (" << accessTech[i % accessTech.size()] << ")";
        anim.UpdateNodeDescription(schoolRouters.Get(i), desc.str());
    }
    
    // Couleurs pour les nœuds (optionnel)
    anim.UpdateNodeColor(cloudServer.Get(0), 0, 0, 255);  // Bleu pour cloud
    anim.UpdateNodeColor(wanRouter.Get(0), 255, 165, 0);  // Orange pour WAN router
    for (uint32_t i = 0; i < nSchools; i++) {
        anim.UpdateNodeColor(schoolRouters.Get(i), 0, 255, 0);  // Vert pour écoles
    }
    
    // Configuration FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }
    
    // ========== LANCEMENT DE LA SIMULATION ==========
    
    NS_LOG_INFO("Démarrage de la simulation...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // ========== STATISTIQUES FINALES ==========
    
    if (enableFlowMonitor) {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
        
        NS_LOG_INFO("\n=== Statistiques de Performance du Réseau ===");
        
        double totalThroughput = 0.0;
        uint64_t totalPacketsSent = 0;
        uint64_t totalPacketsReceived = 0;
        double totalDelay = 0.0;
        uint32_t flowCount = 0;
        
        for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
            
            double throughput = iter->second.rxBytes * 8.0 / simulationTime / 1000000.0;  // Mbps
            totalThroughput += throughput;
            totalPacketsSent += iter->second.txPackets;
            totalPacketsReceived += iter->second.rxPackets;
            
            if (iter->second.rxPackets > 0) {
                double avgDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
                totalDelay += avgDelay;
                flowCount++;
            }
        }
        
        NS_LOG_INFO("Débit total: " << totalThroughput << " Mbps");
        NS_LOG_INFO("Paquets envoyés: " << totalPacketsSent);
        NS_LOG_INFO("Paquets reçus: " << totalPacketsReceived);
        if (totalPacketsSent > 0) {
            NS_LOG_INFO("Taux de livraison: " << (totalPacketsReceived * 100.0 / totalPacketsSent) << "%");
        }
        if (flowCount > 0) {
            NS_LOG_INFO("Délai moyen: " << (totalDelay / flowCount * 1000) << " ms");
        }
        
        // Sauvegarde des statistiques détaillées
        monitor->SerializeToXmlFile("yaounde-flowmon.xml", true, true);
    }
    
    NS_LOG_INFO("\n=== Fin de la simulation ===");
    NS_LOG_INFO("Fichier d'animation NetAnim: " << animFile);
    NS_LOG_INFO("Fichiers PCAP générés dans le répertoire courant");
    NS_LOG_INFO("\nPour visualiser l'animation:");
    NS_LOG_INFO("  netanim " << animFile);
    
    Simulator::Destroy();
    return 0;
}