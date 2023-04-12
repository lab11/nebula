terraform {
    backend "gcs" {
        bucket = "319f5ec7fc212b14-bucket-tfstate"
        prefix = "terraform/state"
    }
}