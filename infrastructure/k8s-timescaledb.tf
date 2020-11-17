resource "kubernetes_pod" "timescaledb" {
  metadata {
    name = "hetida4office-timescaledb"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
      application = "hetida4office"
      service = "timescaledb"
    }
  }
  spec {
    container {
      image = "timescale/timescaledb:latest-pg12"
      name  = "hetida4office-timescaledb"
      port {
        container_port = 5432
      }
      env {
        name = "POSTGRES_DB"
        value = "h4o"
      }
      env {
        name = "POSTGRES_USER"
        value = "admin"
      }
      env {
        name = "POSTGRES_PASSWORD"
        value = "admin"
      }
      volume_mount {
        name = "timescaledb-data"
        mount_path = "/var/lib/postgresql/data/"
        sub_path = "pgdata"
      }
    }
    volume {
      name = "timescaledb-data"
      azure_disk {
          caching_mode = "None"
          kind = "Managed"
          disk_name = "hetida4office"
          data_disk_uri = azurerm_managed_disk.h4o.id
      }
    }
  }
}

resource "kubernetes_service" "timescaledb" {
  metadata {
    name = "hetida4office-timescaledb"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
        application = "hetida4office"
        service = "timescaledb"
    }
  }
  spec {
    selector = {
      service = "timescaledb"
    }
    port {
      port        = 5432
      target_port = 5432
    }
    type = "ClusterIP"
  }
}