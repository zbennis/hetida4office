provider "azurerm" {
  version = "~> 2.0"
  features {}
}

resource "azurerm_resource_group" "h4o" {
  name     = "hetida4office-rg"
  location = "West Europe"

  tags = {
    environment = "Demo"
  }
}

resource "azurerm_kubernetes_cluster" "h4o" {
  name                = "hetida4office-aks"
  location            = azurerm_resource_group.h4o.location
  resource_group_name = azurerm_resource_group.h4o.name
  dns_prefix          = "hetida4office-k8s"

  default_node_pool {
    name            = "h4o"
    node_count      = 1
    vm_size         = "Standard_D2_v3"
    os_disk_size_gb = 30
  }

  service_principal {
    client_id     = var.appId
    client_secret = var.password
  }

  role_based_access_control {
    enabled = true
  }

  addon_profile {
    kube_dashboard {
      enabled = true
    }
  }

  tags = {
    environment = "Demo"
  }
}
